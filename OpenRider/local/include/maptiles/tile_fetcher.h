/// @file
/// Tile fetching for one map layer.
#pragma once

#include "maptiles/decode.h"
#include "maptiles/service_state.h"
#include "maptiles/tile_cache.h"

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

/// Default number of worker threads (connections) per fetcher.
constexpr int kFetchWorkers = 4;
/// Most requests kept waiting; older ones are dropped first.
constexpr size_t kMaxPending = 256;

/// Address of a tile: zoom `z`, column `x`, row `y` (XYZ scheme).
struct TileKey {
    int z = 0;  ///< zoom level
    int x = 0;  ///< column, from the west
    int y = 0;  ///< row, from the north
    /// Orders by zoom, then column, then row.
    bool operator<(const TileKey &o) const { return std::tie(z, x, y) < std::tie(o.z, o.x, o.y); }
    /// Same zoom, column and row.
    bool operator==(const TileKey &o) const { return z == o.z && x == o.x && y == o.y; }
};

/// A requested tile, decoded and ready for the GUI.
struct ReadyTile {
    TileKey key;  ///< the tile
    Image image;  ///< empty when the service has no such tile or it failed
};

/// Tile fetcher for one layer: a pool of worker threads, each with its own
/// libcurl connection, serving the most recent requests first. A tile is
/// taken from the disk cache when present; otherwise it is downloaded
/// politely (see ServiceState), cached and decoded. Decoded tiles wait in a
/// queue the GUI drains on its own thread (textures are GUI business).
class TileFetcher {
public:
    /// Starts the workers.
    /// @param tiles_vtable the tiles plugin's vtable
    /// @param layer the plugin's open layer handle; must outlive the fetcher
    /// @param cache disk cache; must outlive the fetcher
    /// @param user_agent HTTP User-Agent sent to the service
    /// @param workers number of worker threads (connections)
    TileFetcher(const or_tiles_vtable *tiles_vtable, void *layer, TileCache *cache,
                std::string user_agent, int workers = kFetchWorkers);

    /// Stops and joins the workers; tiles in flight are discarded.
    ~TileFetcher();

    /// Queues a tile; ignored if already pending or in flight.
    void Request(const TileKey &key);

    /// Takes the tiles finished since the last call.
    std::vector<ReadyTile> TakeReady();

    /// Tiles that could not be loaded or decoded so far.
    size_t Errors() const { return errors_.load(); }

    /// Tiles pending or in flight.
    size_t Pending() const;

    /// The service is unavailable: every server keeps failing.
    bool Unavailable() const;

private:
    /// Worker thread body: serves requests until stopped.
    void Worker();

    /// Downloads `key` politely and caches it.
    /// @param curl the worker's CURL easy handle
    /// @param key the tile
    /// @param bytes receives the tile file
    /// @param missing set when the service has no tile there (not a fault)
    /// @return the tile was downloaded
    bool FetchNetwork(void *curl, const TileKey &key, std::string &bytes, bool &missing);

    const or_tiles_vtable *tiles_vtable_;  ///< tiles plugin
    void *layer_;                          ///< its open layer
    const or_tile_layer *layer_info_;      ///< the layer's description
    TileCache *cache_;                     ///< disk cache
    std::string user_agent_;               ///< HTTP User-Agent

    mutable std::mutex mutex_;         ///< guards the queue, requested_, ready_ and stop_
    std::condition_variable wake_;     ///< new request, or stopping
    std::deque<TileKey> queue_;        ///< waiting requests, newest at the back
    std::set<TileKey> requested_;      ///< pending or in flight
    std::vector<ReadyTile> ready_;     ///< finished, not yet taken
    bool stop_ = false;                ///< asks the workers to stop

    mutable std::mutex service_mutex_;  ///< guards service_
    ServiceState service_;              ///< polite fetching state of the service

    std::atomic<size_t> errors_{0};     ///< tiles that failed
    std::vector<std::thread> threads_;  ///< the workers
};

} // namespace maptiles
