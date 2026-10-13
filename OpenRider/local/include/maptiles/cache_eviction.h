/// @file
/// Least-recently-used eviction of the tile disk cache (module-level
/// utilities).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace maptiles {

/// One file of the tile cache.
struct CacheEntry {
    std::string path;    ///< file path
    uint64_t size = 0;   ///< bytes
    double mtime = 0.0;  ///< last use (s), sub-second resolution so same-second writes still order
};

/// Oldest entries to delete so that the total size fits in `max_bytes`.
/// @return paths of the victims, oldest first
std::vector<std::string> LruVictims(std::vector<CacheEntry> entries, uint64_t max_bytes);

} // namespace maptiles
