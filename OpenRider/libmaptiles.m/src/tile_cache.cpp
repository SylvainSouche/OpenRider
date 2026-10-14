#include "maptiles/tile_cache.h"

#include "core/paths.h"
#include "maptiles/cache_eviction.h"

#include <atomic>
#include <cstdio>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

namespace maptiles {

namespace {

// Appends every tile file under `dir` (recursively) to `out`.
void ScanTiles(const std::string &dir, std::vector<CacheEntry> &out)
{
    DIR *directory = opendir(dir.c_str());
    if (!directory)
        return;
    while (dirent *item = readdir(directory)) {
        const std::string name = item->d_name;
        if (name == "." || name == "..")
            continue;
        const std::string path = dir + "/" + name;
        struct stat st;
        if (stat(path.c_str(), &st) != 0)
            continue;
        if (S_ISDIR(st.st_mode))
            ScanTiles(path, out);
        else if (name.size() > 5 && name.compare(name.size() - 5, 5, ".tile") == 0) {
#ifdef __APPLE__
            const double mtime = st.st_mtimespec.tv_sec + st.st_mtimespec.tv_nsec * 1e-9;
#else
            const double mtime = st.st_mtim.tv_sec + st.st_mtim.tv_nsec * 1e-9;
#endif
            out.push_back(CacheEntry{path, static_cast<uint64_t>(st.st_size), mtime});
        }
    }
    closedir(directory);
}

} // namespace

TileCache::TileCache(std::string root, uint64_t max_bytes)
    : root_(std::move(root)), max_bytes_(max_bytes)
{
}

std::string TileCache::Path(const std::string &layer, int z, int x, int y) const
{
    return root_ + "/" + layer + "/" + std::to_string(z) + "/" + std::to_string(x) + "/" +
           std::to_string(y) + ".tile";
}

bool TileCache::Get(const std::string &layer, int z, int x, int y, std::string &bytes)
{
    const std::string path = Path(layer, z, x, y);
    FILE *file = std::fopen(path.c_str(), "rb");
    if (!file)
        return false;
    bytes.clear();
    char chunk[16384];
    size_t chunk_size;
    while ((chunk_size = std::fread(chunk, 1, sizeof chunk, file)) > 0)
        bytes.append(chunk, chunk_size);
    std::fclose(file);
    utimes(path.c_str(), nullptr);  // most recently used now
    return !bytes.empty();
}

// @impl 0f87-6acd-2f33-dc20
openrider::sdk::Status TileCache::Put(const std::string &layer, int z, int x, int y, const std::string &bytes)
{
    const std::string path = Path(layer, z, x, y);
    const std::string dir = path.substr(0, path.find_last_of('/'));
    if (!core::MakeDirs(dir))
        return openrider::sdk::Status::Failure("cannot create cache directory " + dir);
    // Atomic: write a temporary file, then rename it into place. The name is
    // unique per process and per call, so concurrent writers never collide.
    static std::atomic<uint64_t> writes{0};
    const std::string temporary = path + ".tmp" + std::to_string(getpid()) + "-" + std::to_string(++writes);
    FILE *file = std::fopen(temporary.c_str(), "wb");
    if (!file)
        return openrider::sdk::Status::Failure("cannot write " + temporary);
    const bool written = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
    if (std::fclose(file) != 0 || !written || std::rename(temporary.c_str(), path.c_str()) != 0) {
        std::remove(temporary.c_str());
        return openrider::sdk::Status::Failure("cannot store tile " + path);
    }
    // The running total avoids a full scan per write; the first write after
    // start-up scans once to learn the real size.
    bool over_maximum = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        total_ += bytes.size();
        written_ += bytes.size();
        over_maximum = !total_known_ || total_ > max_bytes_;
    }
    if (over_maximum)
        Evict();
    return openrider::sdk::Status::Success();
}

void TileCache::Evict()
{
    std::unique_lock<std::mutex> evicting(evict_mutex_, std::try_to_lock);
    if (!evicting.owns_lock())
        return;  // another thread is evicting
    uint64_t written_before = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        written_before = written_;
    }
    // The scan runs outside mutex_, so that writers are not held up; the
    // tiles they write meanwhile are added back to the total below.
    std::vector<CacheEntry> entries;
    ScanTiles(root_, entries);
    std::unordered_map<std::string, uint64_t> size_of;
    uint64_t total = 0;
    for (const CacheEntry &entry : entries) {
        size_of[entry.path] = entry.size;
        total += entry.size;
    }
    for (const std::string &victim : LruVictims(entries, max_bytes_))
        if (std::remove(victim.c_str()) == 0)
            total -= size_of[victim];
    std::lock_guard<std::mutex> lock(mutex_);
    total_ = total + (written_ - written_before);
    total_known_ = true;
}

} // namespace maptiles
