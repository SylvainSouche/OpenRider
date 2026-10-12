#include "maptiles/cache.h"

#include "core/paths.h"

#include <algorithm>
#include <cstdio>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

namespace maptiles {

namespace {

void scan(const std::string &dir, std::vector<CacheEntry> &out)
{
    DIR *d = opendir(dir.c_str());
    if (!d)
        return;
    while (dirent *e = readdir(d)) {
        const std::string name = e->d_name;
        if (name == "." || name == "..")
            continue;
        const std::string p = dir + "/" + name;
        struct stat st;
        if (stat(p.c_str(), &st) != 0)
            continue;
        if (S_ISDIR(st.st_mode))
            scan(p, out);
        else if (name.size() > 5 && name.compare(name.size() - 5, 5, ".tile") == 0) {
#ifdef __APPLE__
            const double mtime = st.st_mtimespec.tv_sec + st.st_mtimespec.tv_nsec * 1e-9;
#else
            const double mtime = st.st_mtim.tv_sec + st.st_mtim.tv_nsec * 1e-9;
#endif
            out.push_back(CacheEntry{p, static_cast<uint64_t>(st.st_size), mtime});
        }
    }
    closedir(d);
}

} // namespace

std::vector<std::string> lru_victims(std::vector<CacheEntry> entries, uint64_t max_bytes)
{
    uint64_t total = 0;
    for (const CacheEntry &e : entries)
        total += e.size;
    std::stable_sort(entries.begin(), entries.end(),
              [](const CacheEntry &a, const CacheEntry &b) { return a.mtime < b.mtime; });
    std::vector<std::string> victims;
    for (const CacheEntry &e : entries) {
        if (total <= max_bytes)
            break;
        victims.push_back(e.path);
        total -= e.size;
    }
    return victims;
}

TileCache::TileCache(std::string root, uint64_t max_bytes)
    : root_(std::move(root)), max_bytes_(max_bytes)
{
}

std::string TileCache::path(const std::string &layer, int z, int x, int y) const
{
    return root_ + "/" + layer + "/" + std::to_string(z) + "/" + std::to_string(x) + "/" +
           std::to_string(y) + ".tile";
}

bool TileCache::get(const std::string &layer, int z, int x, int y, std::string &bytes)
{
    const std::string p = path(layer, z, x, y);
    FILE *f = std::fopen(p.c_str(), "rb");
    if (!f)
        return false;
    bytes.clear();
    char buf[16384];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0)
        bytes.append(buf, n);
    std::fclose(f);
    utimes(p.c_str(), nullptr);  // most recently used now
    return !bytes.empty();
}

// @impl 0f87-6acd-2f33-dc20
core::Status TileCache::put(const std::string &layer, int z, int x, int y, const std::string &bytes)
{
    const std::string p = path(layer, z, x, y);
    const std::string dir = p.substr(0, p.find_last_of('/'));
    if (!core::make_dirs(dir))
        return core::Status::failure("cannot create cache directory " + dir);
    // Atomic: write a temporary file, then rename it into place.
    const std::string tmp = p + ".tmp" + std::to_string(getpid()) + "-" +
                            std::to_string(reinterpret_cast<uintptr_t>(&bytes));
    FILE *f = std::fopen(tmp.c_str(), "wb");
    if (!f)
        return core::Status::failure("cannot write " + tmp);
    const bool ok = std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
    if (std::fclose(f) != 0 || !ok || std::rename(tmp.c_str(), p.c_str()) != 0) {
        std::remove(tmp.c_str());
        return core::Status::failure("cannot store tile " + p);
    }
    bool over = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        total_ += bytes.size();
        over = !total_known_ || total_ > max_bytes_;
    }
    if (over)
        evict();
    return core::Status::success();
}

void TileCache::evict()
{
    std::vector<CacheEntry> entries;
    scan(root_, entries);
    uint64_t total = 0;
    for (const CacheEntry &e : entries)
        total += e.size;
    for (const std::string &v : lru_victims(entries, max_bytes_)) {
        for (const CacheEntry &e : entries)
            if (e.path == v)
                total -= e.size;
        std::remove(v.c_str());
    }
    std::lock_guard<std::mutex> lock(mutex_);
    total_ = total;
    total_known_ = true;
}

} // namespace maptiles
