// Map view: tiles of one layer as OpenGL textures, the outing's track on
// top, pan by dragging, zoom with the mouse wheel, attribution on the map.
#pragma once

#include "core/overview.h"
#include "maptiles/cache.h"
#include "maptiles/coverage.h"
#include "maptiles/fetcher.h"

#include <openrider/input.h>
#include <openrider/tiles.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

struct MapTexture {
    unsigned int gl = 0;      // 0: the service has no such tile
    unsigned long last_used = 0;
};

// A layer definition file the user can choose as background.
struct MapLayerChoice {
    std::string json;
    std::string id, name;
    double extent[4] = {-180, -85.0511287798, 180, 85.0511287798};
};

struct MapView {
    std::vector<MapLayerChoice> layers;  // every readable layer file
    int current = -1;                    // index into layers

    const or_tiles_vtable *vt = nullptr;
    void *layer = nullptr;
    const or_tile_layer *info = nullptr;
    std::unique_ptr<maptiles::TileCache> cache;
    std::unique_ptr<maptiles::TileFetcher> fetcher;

    std::map<maptiles::TileKey, MapTexture> textures;
    std::map<maptiles::TileKey, double> retry_after;  // failed tiles, by time
    unsigned long frame = 0;

    int zoom = 3;
    double cx = 0.0, cy = 0.0;      // view centre, world pixels at `zoom`
    std::string fitted_id;          // outing the view was last fitted to
    bool unavailable_reported = false;
};

// How the track is drawn: one colour, or coloured by a value per point
// (NaN = no value) with a gradient between the 5th and 95th percentiles.
struct TrackPaint {
    const std::vector<double> *values = nullptr;  // null: solid colour
    const char *label = "";                       // legend title, e.g. "Speed (km/h)"
    unsigned int solid = 0xE61E1EDC;              // ImU32 (ABGR)
    int hover_index = -1;                         // point to mark, -1 = none
};

// Reads the layer files (*.json) of `dirs`, in order; returns problems.
std::vector<std::string> map_load_layers(MapView &m, const std::vector<std::string> &dirs);
// Switches the background to layers[index], keeping the view centre; "" on
// success.
std::string map_select(MapView &m, int index);
// Layers to offer now: those covering the view and the track (the current
// one always included).
std::vector<int> map_offered_layers(const MapView &m, const std::vector<or_point> &points,
                                    float width, float height);
// Draws the map in a region of `size`; returns a message when the layer
// has just become unavailable ("" otherwise).
std::string map_draw(MapView &m, const std::vector<or_point> &points, const std::string &outing_id,
                     float width, float height, const TrackPaint &paint);
void map_close(MapView &m);

// The overview: the given tracks at once (each in its colour, ImU32), the
// one under the mouse highlighted.
// `fit` frames all tracks first. Returns the id of a clicked track ("" if
// none); sets `hovered_id`, and `message` when the layer just became
// unavailable.
std::string map_draw_overview(MapView &m, const std::vector<const core::OverviewTrack *> &tracks,
                              const std::vector<unsigned int> &colours,
                              float width, float height, bool fit, std::string &hovered_id,
                              std::string &message);
// The region shown, in WGS84 degrees.
maptiles::GeoBox map_view_box(const MapView &m, float width, float height);

// View centre and zoom, kept across modes (overview / one outing).
struct MapViewState {
    double lon = 0.0, lat = 0.0;
    int zoom = 0;
    bool valid = false;
};
MapViewState map_get_state(const MapView &m);
void map_set_state(MapView &m, const MapViewState &st);
