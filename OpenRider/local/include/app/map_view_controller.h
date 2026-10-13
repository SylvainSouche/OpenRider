/// @file
/// The map's presentation: layers, cameras, tiles, track style, hover.
#pragma once

#include "app/app_model.h"
#include "app/intents.h"

#include "maptiles/tile_cache.h"
#include "maptiles/tile_fetcher.h"

#include <openrider/tiles.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace app {

/// Highest zoom of the view; beyond the layer's last level, tiles are enlarged.
constexpr int kMaxViewZoom = 20;

/// Where the map looks.
struct MapCamera {
    double lon = 6.63;               ///< view centre, WGS84 longitude
    double lat = 45.94;              ///< view centre, WGS84 latitude
    int zoom = 3;                    ///< view zoom level
    bool fitted = false;             ///< fitted once to its content
};

/// A map layer as offered to the user.
struct MapLayerInfo {
    std::string json;         ///< layer definition, given to the tiles plugin
    std::string id;           ///< stable identifier
    std::string name;         ///< display name
    std::string attribution;  ///< credit line shown on the map
    double extent[4] = {-180, -85.0511287798, 180, 85.0511287798};  ///< west, south, east, north
    int min_zoom = 0;     ///< first zoom level served
    int max_zoom = 18;    ///< last zoom level served
    int tile_size = 256;  ///< tile width and height, pixels
};

/// A decoded tile ready to draw.
struct MapTile {
    maptiles::Image image;         ///< decoded RGBA
    unsigned long generation = 0;  ///< changes when the image is replaced
    unsigned long last_used = 0;   ///< frame it was last shown in
};

/// What the map shows: every outing, or the selected one.
enum class MapMode { Overview, Outing, Comparison };

/// What the map view keeps of its presentation.
struct MapViewModel {
    std::vector<MapLayerInfo> layers;  ///< every usable layer
    int current = -1;                  ///< index into layers, -1 = none

    MapMode mode = MapMode::Overview;  ///< follows the model's selection
    MapCamera overview;                ///< camera of the overview
    MapCamera outing;                  ///< camera of the selected outing
    MapCamera comparison;              ///< camera of the segment comparison
    float width = 0.0f;                ///< viewport width, pixels
    float height = 0.0f;               ///< viewport height, pixels

    std::map<maptiles::TileKey, MapTile> tiles;  ///< decoded, ready to draw
    size_t tile_errors = 0;    ///< tiles that failed so far
    size_t tiles_pending = 0;  ///< tiles requested, not yet ready

    int track_style = 0;                      ///< 0 = solid colour, see kTrackStyles
    unsigned int track_colour = 0xE61E1EDCu;  ///< ImU32 (ABGR)
    std::string hovered_track;                ///< overview: outing id under the mouse
    int hovered_group = -1;                   ///< overview with segments: AppModel::segment_groups index, -1 = none
    int hovered_segment = -1;                 ///< its segment id under the mouse, -1 = none
    std::string fitted_outing;                ///< outing the outing camera was fitted to
    unsigned fitted_comparison = 0;           ///< comparison generation the comparison camera was fitted to

    /// The camera of the current mode.
    const MapCamera &Camera() const
    {
        return mode == MapMode::Overview ? overview : mode == MapMode::Outing ? outing : comparison;
    }

    /// The current layer, or null.
    const MapLayerInfo *Layer() const
    {
        return current >= 0 && current < static_cast<int>(layers.size()) ? &layers[static_cast<size_t>(current)]
                                                                           : nullptr;
    }
};

/// Owns the MapViewModel and the tile services (tiles plugins, disk cache,
/// fetcher); changes the view model in response to map intents and to
/// tiles arriving.
class MapViewController {
public:
    MapViewController();

    /// Stops the fetcher and closes the layer.
    ~MapViewController();

    /// The view model, for the map view.
    const MapViewModel &ViewModel() const { return view_model_; }

    /// Loads the layer files of `dirs` through the tiles plugins found in
    /// `plugin_roots` and selects `default_id` if present, else the first.
    /// @param dirs directories holding layer files (*.json)
    /// @param plugin_roots plugin roots searched for tiles plugins
    /// @param cache_dir root of the tile disk cache
    /// @param default_id layer shown first, when present
    /// @return problems found, one message each
    std::vector<std::string> LoadLayers(const std::vector<std::string> &dirs,
                                        const std::vector<std::string> &plugin_roots,
                                        const std::string &cache_dir, const std::string &default_id);

    /// @name The same as LoadLayers(), one piece at a time (tests)
    /// @{

    /// Adds a tiles protocol.
    void AddPlugin(const or_tiles_vtable *tiles_vtable) { plugins_.push_back(tiles_vtable); }

    /// Adds a layer definition; false if no protocol accepts it.
    bool AddLayer(const std::string &json);

    /// Sets the tile cache root.
    void SetCacheDir(const std::string &dir) { cache_dir_ = dir; }

    /// Shows layer `index`.
    /// @return "" or why the layer cannot be opened
    std::string Select(int index);
    /// @}

    /// Once per frame: follows the model's mode, fits the cameras, takes
    /// ready tiles, requests the visible ones, publishes the visible tracks.
    /// @param model the model, read-only
    /// @param out receives follow-up intents
    void Tick(const AppModel &model, Intents &out);

    /// Handles the map's intents (see IntentKind).
    /// @param intent the intent to handle
    /// @param model the model, read-only
    /// @param out receives follow-up intents
    void Handle(const Intent &intent, const AppModel &model, Intents &out);

    /// Stops the fetcher and closes the layer.
    void Shutdown();

private:
    /// Fits the outing camera to the selected outing's points.
    void FitOuting(const AppModel &model);

    /// Fits the comparison camera to the compared segment and its passages.
    void FitComparison(const AppModel &model);

    /// Fits the overview camera to every track.
    void FitOverview(const AppModel &model);

    /// Requests the tiles covering the view that are not decoded yet.
    void RequestVisibleTiles();

    /// Overview: finds the track under the mouse, or the segment when the
    /// segment networks are shown.
    /// @param model the model, read-only
    /// @param mouse_x mouse offset from the map centre, pixels, rightwards
    /// @param mouse_y mouse offset from the map centre, pixels, downwards
    void UpdateHover(const AppModel &model, double mouse_x, double mouse_y);

    /// Overview with segments: finds the segment under the mouse.
    /// @param model the model, read-only
    /// @param mouse_x mouse offset from the map centre, pixels, rightwards
    /// @param mouse_y mouse offset from the map centre, pixels, downwards
    void UpdateSegmentHover(const AppModel &model, double mouse_x, double mouse_y);

    /// Overview: pushes SetVisibleIds when the tracks in view change.
    void PublishVisible(const AppModel &model, Intents &out);

    MapViewModel view_model_;                       ///< changed only here
    std::vector<const or_tiles_vtable *> plugins_;  ///< tiles protocols
    const or_tiles_vtable *tiles_vtable_ = nullptr; ///< protocol of the open layer
    void *layer_ = nullptr;                         ///< open layer handle
    std::string cache_dir_;                            ///< root of the tile disk cache
    std::unique_ptr<maptiles::TileCache> cache_;       ///< disk cache of the open layer
    std::unique_ptr<maptiles::TileFetcher> fetcher_;   ///< fetcher of the open layer
    std::map<maptiles::TileKey, double> retry_after_;  ///< missing tiles: when to ask again (s)
    unsigned long frame_ = 0;            ///< frames ticked, for least-recently-shown eviction
    unsigned long generation_ = 0;       ///< last MapTile::generation given
    bool unavailable_reported_ = false;  ///< the unavailable service was reported
    std::vector<std::string> last_visible_;  ///< last published SetVisibleIds
};

} // namespace app
