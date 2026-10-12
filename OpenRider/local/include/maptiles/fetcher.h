// Tile fetcher for one layer: a pool of worker threads, each with its own
// libcurl connection, serving the most recent requests first. A tile is
// taken from the disk cache when present; otherwise it is downloaded
// politely (see policy.h), cached and decoded. Decoded tiles wait in a
// queue the GUI drains on its own thread (textures are GUI business).
#pragma once

#include "maptiles/cache.h"
#include "maptiles/decode.h"
#include "maptiles/policy.h"

#include <openrider/tiles.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

namespace maptiles {

constexpr int kFetchWorkers = 4;
constexpr size_t kMaxPending = 256;

struct TileKey {
    int z = 0, x = 0, y = 0;
    bool operator<(const TileKey &o) const { return std::tie(z, x, y) < std::tie(o.z, o.x, o.y); }
    bool operator==(const TileKey &o) const { return z == o.z && x == o.x && y == o.y; }
};

struct ReadyTile {
    TileKey key;
    Image image;   // empty when the service has no such tile
};

class TileFetcher {
public:
    TileFetcher(const or_tiles_vtable *vt, void *layer, TileCache *cache, std::string user_agent,
                int workers = kFetchWorkers);
    ~TileFetcher();

    void request(const TileKey &k);  // ignored if already pending or in flight
    std::vector<ReadyTile> take_ready();

    size_t errors() const { return errors_.load(); }
    size_t pending() const;
    bool unavailable() const;

private:
    void worker();
    bool fetch_network(void *curl, const TileKey &k, std::string &bytes, bool &missing);

    const or_tiles_vtable *vt_;
    void *layer_;
    const or_tile_layer *info_;
    TileCache *cache_;
    std::string user_agent_;

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<TileKey> queue_;       // newest at the back
    std::set<TileKey> requested_;     // pending or in flight
    std::vector<ReadyTile> ready_;
    bool stop_ = false;

    mutable std::mutex service_mutex_;
    ServiceState service_;

    std::atomic<size_t> errors_{0};
    std::vector<std::thread> threads_;
};

} // namespace maptiles
