// Disk cache of raw tile files, one directory per layer:
// <root>/<layer>/<z>/<x>/<y>.tile. Writes are atomic (temporary file, then
// rename). Reading a tile refreshes its modification time, which orders the
// least-recently-used eviction applied above the maximum size.
#pragma once

#include "core/result.h"

#include <cstdint>
#include <ctime>
#include <mutex>
#include <string>
#include <vector>

namespace maptiles {

constexpr uint64_t kDefaultCacheBytes = 2ull * 1024 * 1024 * 1024;  // 2 GB

struct CacheEntry {
    std::string path;
    uint64_t size = 0;
    double mtime = 0.0;  // seconds, sub-second resolution: same-second writes still order
};

// Oldest entries to delete so the total fits in max_bytes.
std::vector<std::string> lru_victims(std::vector<CacheEntry> entries, uint64_t max_bytes);

class TileCache {
public:
    TileCache(std::string root, uint64_t max_bytes);

    std::string path(const std::string &layer, int z, int x, int y) const;
    bool get(const std::string &layer, int z, int x, int y, std::string &bytes);
    core::Status put(const std::string &layer, int z, int x, int y, const std::string &bytes);
    void evict();  // full scan; called when the running total exceeds the maximum

private:
    std::string root_;
    uint64_t max_bytes_;
    std::mutex mutex_;
    uint64_t total_ = 0;
    bool total_known_ = false;
};

} // namespace maptiles
