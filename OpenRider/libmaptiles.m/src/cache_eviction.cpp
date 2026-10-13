#include "maptiles/cache_eviction.h"

#include <algorithm>

namespace maptiles {

std::vector<std::string> LruVictims(std::vector<CacheEntry> entries, uint64_t max_bytes)
{
    uint64_t total = 0;
    for (const CacheEntry &entry : entries)
        total += entry.size;
    std::stable_sort(entries.begin(), entries.end(),
                     [](const CacheEntry &a, const CacheEntry &b) { return a.mtime < b.mtime; });
    std::vector<std::string> victims;
    for (const CacheEntry &entry : entries) {
        if (total <= max_bytes)
            break;
        victims.push_back(entry.path);
        total -= entry.size;
    }
    return victims;
}

} // namespace maptiles
