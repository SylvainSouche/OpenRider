/// @file
/// Disk cache of raw tile files.
#pragma once

#include <openrider/sdk/result.h>

#include <cstdint>
#include <mutex>
#include <string>

namespace maptiles {

/// Default maximum size of the tile cache: 2 GB.
constexpr uint64_t kDefaultCacheBytes = 2ull * 1024 * 1024 * 1024;

/// Disk cache of raw tile files, one directory per layer:
/// `<root>/<layer>/<z>/<x>/<y>.tile`. Writes are atomic (temporary file, then
/// rename). Reading a tile refreshes its modification time, which orders the
/// least-recently-used eviction applied above the maximum size.
/// Thread-safe.
class TileCache {
public:
    /// @param root cache directory, created on the first write
    /// @param max_bytes size above which the least recently used tiles are deleted
    TileCache(std::string root, uint64_t max_bytes);

    /// File path of a tile of `layer` at zoom `z`, column `x`, row `y`.
    std::string Path(const std::string &layer, int z, int x, int y) const;

    /// Reads a cached tile into `bytes` and marks it as just used.
    /// @return the tile was cached and not empty
    bool Get(const std::string &layer, int z, int x, int y, std::string &bytes);

    /// Stores a tile atomically, then evicts when the cache is over its maximum.
    openrider::sdk::Status Put(const std::string &layer, int z, int x, int y, const std::string &bytes);

    /// Scans the whole cache and deletes the least recently used tiles until
    /// it fits; called by Put() when the running total exceeds the maximum.
    /// One eviction at a time: a call while another runs returns at once
    /// (the running one accounts for the tiles written meanwhile).
    void Evict();

private:
    std::string root_;          ///< cache directory
    uint64_t max_bytes_;        ///< size above which tiles are evicted
    std::mutex mutex_;          ///< guards total_, written_ and total_known_
    uint64_t total_ = 0;        ///< running total of the cache size (bytes)
    uint64_t written_ = 0;      ///< bytes written since construction
    bool total_known_ = false;  ///< total_ comes from a full scan
    std::mutex evict_mutex_;    ///< held by the one eviction running
};

} // namespace maptiles
