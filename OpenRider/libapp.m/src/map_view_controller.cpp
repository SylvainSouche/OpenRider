#include "app/map_view_controller.h"

#include "app/map_view_utils.h"

#include "core/log.h"
#include "core/outing_selection.h"
#include "core/plugins.h"
#include "maptiles/mercator.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <dirent.h>

namespace app {

namespace {

constexpr size_t kMaxTiles = 256;        // decoded tiles kept (~64 MB)
constexpr double kRetrySeconds = 30.0;   // before asking again for a missing tile
constexpr float kHoverPx = 6.0f;         // overview: track under the mouse

// Monotonic time (s).
double NowS()
{
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

// The .json files of `dir`, sorted by path.
std::vector<std::string> JsonFiles(const std::string &dir)
{
    std::vector<std::string> paths;
    if (DIR *directory = opendir(dir.c_str())) {
        while (dirent *item = readdir(directory)) {
            const std::string name = item->d_name;
            if (name.size() > 5 && name[0] != '.' && name.compare(name.size() - 5, 5, ".json") == 0)
                paths.push_back(dir + "/" + name);
        }
        closedir(directory);
    }
    std::sort(paths.begin(), paths.end());
    return paths;
}

// Whole content of a file; "" if it cannot be read.
std::string ReadText(const std::string &path)
{
    std::string text;
    if (FILE *file = std::fopen(path.c_str(), "rb")) {
        char chunk[4096];
        size_t chunk_size;
        while ((chunk_size = std::fread(chunk, 1, sizeof chunk, file)) > 0)
            text.append(chunk, chunk_size);
        std::fclose(file);
    }
    return text;
}

// Squared distance from (x, y) to the line segment (ax, ay)-(bx, by).
double Distance2ToSegment(double x, double y, double ax, double ay, double bx, double by)
{
    const double dx = bx - ax, dy = by - ay;
    const double length2 = dx * dx + dy * dy;
    double along = length2 > 0.0 ? ((x - ax) * dx + (y - ay) * dy) / length2 : 0.0;
    along = std::clamp(along, 0.0, 1.0);
    const double px = ax + along * dx - x, py = ay + along * dy - y;
    return px * px + py * py;
}

// Highest view zoom for `layer`: at least kMaxViewZoom (over-zoom).
int MaxViewZoom(const MapLayerInfo *layer)
{
    return layer ? std::max(kMaxViewZoom, layer->max_zoom) : kMaxViewZoom;
}

// Centres `camera` on the box and picks the largest zoom showing it within
// 90% of the viewport, leaving two levels of zoom-in headroom.
void FitCamera(MapCamera &camera, const MapLayerInfo &layer, double west, double south, double east,
               double north, float width, float height)
{
    camera.zoom = maptiles::FitZoom(west, south, east, north, width * 0.9, height * 0.9, layer.tile_size,
                                    layer.min_zoom, std::max(layer.max_zoom, kMaxViewZoom - 2));
    double left, top, right, bottom;
    maptiles::LonlatToWorld(west, north, camera.zoom, layer.tile_size, left, top);
    maptiles::LonlatToWorld(east, south, camera.zoom, layer.tile_size, right, bottom);
    maptiles::WorldToLonlat((left + right) / 2.0, (top + bottom) / 2.0, camera.zoom, layer.tile_size,
                            camera.lon, camera.lat);
    camera.fitted = true;
}

} // namespace

MapViewController::MapViewController() = default;

MapViewController::~MapViewController() { Shutdown(); }

bool MapViewController::AddLayer(const std::string &json)
{
    MapLayerInfo info;
    info.json = json;
    // The first protocol that opens the definition describes the layer.
    for (const or_tiles_vtable *tiles_vtable : plugins_) {
        char error[256] = "";
        void *handle = tiles_vtable->open(info.json.c_str(), error, sizeof error);
        if (!handle)
            continue;
        const or_tile_layer *layer = tiles_vtable->layer(handle);
        info.id = layer->id;
        info.name = layer->name;
        info.attribution = layer->attribution ? layer->attribution : "";
        for (int k = 0; k < 4; ++k)
            info.extent[k] = layer->extent[k];
        info.min_zoom = layer->min_zoom;
        info.max_zoom = layer->max_zoom;
        info.tile_size = layer->tile_size;
        tiles_vtable->close(handle);
        view_model_.layers.push_back(info);
        return true;
    }
    return false;
}

void MapViewController::Shutdown()
{
    fetcher_.reset();  // joins the workers before the layer goes away
    cache_.reset();
    if (tiles_vtable_ && layer_)
        tiles_vtable_->close(layer_);
    layer_ = nullptr;
}

std::vector<std::string> MapViewController::LoadLayers(const std::vector<std::string> &dirs,
                                                       const std::vector<std::string> &plugin_roots,
                                                       const std::string &cache_dir,
                                                       const std::string &default_id)
{
    std::vector<std::string> problems;
    cache_dir_ = cache_dir;
    const core::PluginScan scan = core::LoadPlugins(plugin_roots, OR_PLUGIN_TILES);
    for (const std::string &problem : scan.problems)
        problems.push_back("Plugin refused: " + problem);
    for (const core::LoadedPlugin &plugin : scan.loaded)
        plugins_.push_back(static_cast<const or_tiles_vtable *>(plugin.info->vtable));
    for (const std::string &dir : dirs)
        for (const std::string &path : JsonFiles(dir))
            if (!AddLayer(ReadText(path)))
                problems.push_back("map layer " + path + " is not usable by any tiles plugin");
    if (view_model_.layers.empty()) {
        problems.push_back("No map layer found");
        return problems;
    }
    int initial = 0;
    for (size_t i = 0; i < view_model_.layers.size(); ++i)
        if (view_model_.layers[i].id == default_id)
            initial = static_cast<int>(i);
    const std::string error = Select(initial);
    if (!error.empty())
        problems.push_back("Cannot open the map layer: " + error);
    return problems;
}

// @impl 0f87-6acd-3542-dba2
std::string MapViewController::Select(int index)
{
    if (index < 0 || index >= static_cast<int>(view_model_.layers.size()) || index == view_model_.current)
        return "";
    Shutdown();
    const MapLayerInfo &info = view_model_.layers[static_cast<size_t>(index)];
    std::string why;
    for (const or_tiles_vtable *tiles_vtable : plugins_) {
        char error[256] = "";
        void *handle = tiles_vtable->open(info.json.c_str(), error, sizeof error);
        if (!handle) {
            why += error;
            continue;
        }
        tiles_vtable_ = tiles_vtable;
        layer_ = handle;
        break;
    }
    if (!layer_)
        return why.empty() ? "no tiles plugin accepts it" : why;
    cache_ = std::make_unique<maptiles::TileCache>(cache_dir_, maptiles::kDefaultCacheBytes);
    fetcher_ = std::make_unique<maptiles::TileFetcher>(tiles_vtable_, layer_, cache_.get(),
                                                       "OpenRider/0.1 (desktop outing viewer; libcurl)");
    view_model_.current = index;
    view_model_.tiles.clear();
    retry_after_.clear();
    unavailable_reported_ = false;
    for (MapCamera *camera : {&view_model_.overview, &view_model_.outing})
        camera->zoom = std::clamp(camera->zoom, info.min_zoom, MaxViewZoom(&info));
    core::LogLine("map layer %s", info.name.c_str());
    return "";
}

void MapViewController::FitOuting(const AppModel &model)
{
    double west = 180, east = -180, south = 90, north = -90;
    for (const or_point &point : model.points) {
        if (std::isnan(point.lat) || std::isnan(point.lon))
            continue;
        west = std::min(west, point.lon);
        east = std::max(east, point.lon);
        south = std::min(south, point.lat);
        north = std::max(north, point.lat);
    }
    if (west <= east && view_model_.Layer())
        FitCamera(view_model_.outing, *view_model_.Layer(), west, south, east, north, view_model_.width,
                  view_model_.height);
    view_model_.fitted_outing = model.points_id;
}

void MapViewController::FitOverview(const AppModel &model)
{
    double west = 180, east = -180, south = 90, north = -90;
    for (const core::OverviewTrack &track : model.tracks) {
        west = std::min(west, track.west);
        east = std::max(east, track.east);
        south = std::min(south, track.south);
        north = std::max(north, track.north);
    }
    if (west <= east && view_model_.Layer())
        FitCamera(view_model_.overview, *view_model_.Layer(), west, south, east, north, view_model_.width,
                  view_model_.height);
}

// Requests the tiles covering the view: from level min(view zoom, layer
// max); past the layer's last level the view enlarges them (over-zoom).
// @impl 0f87-6acd-5daf-1652
void MapViewController::RequestVisibleTiles()
{
    const MapLayerInfo *layer = view_model_.Layer();
    if (!layer || !fetcher_ || view_model_.width <= 0.0f)
        return;
    const MapCamera &camera = view_model_.Camera();
    const int tile_zoom = std::min(camera.zoom, layer->max_zoom);
    // Size on screen of one tile of that level (larger than tile_size when
    // over-zoomed), and the number of tiles across the world.
    const double tile_px = layer->tile_size * std::pow(2.0, camera.zoom - tile_zoom);
    const int tiles_across = 1 << tile_zoom;
    double left, top;
    MapOrigin(view_model_, left, top);
    const double now = NowS();
    const int first_row = std::max(0, static_cast<int>(std::floor(top / tile_px)));
    const int last_row = std::min(tiles_across - 1, static_cast<int>(std::floor((top + view_model_.height) / tile_px)));
    const int first_column = static_cast<int>(std::floor(left / tile_px));
    const int last_column = static_cast<int>(std::floor((left + view_model_.width) / tile_px));
    for (int row = first_row; row <= last_row; ++row)
        for (int column = first_column; column <= last_column; ++column) {
            // Columns wrap around the antimeridian.
            const maptiles::TileKey key{tile_zoom, ((column % tiles_across) + tiles_across) % tiles_across, row};
            auto decoded = view_model_.tiles.find(key);
            if (decoded != view_model_.tiles.end()) {
                decoded->second.last_used = frame_;
                continue;
            }
            auto retry = retry_after_.find(key);
            if (retry == retry_after_.end() || retry->second <= now)
                fetcher_->Request(key);
        }
}

void MapViewController::PublishVisible(const AppModel &model, Intents &out)
{
    const maptiles::GeoBox box = MapViewBox(view_model_);
    std::vector<std::string> ids;
    for (const core::OverviewTrack &track : model.tracks)
        if (core::TrackInBox(track, box.west, box.south, box.east, box.north))
            ids.push_back(track.id);
    if (ids != last_visible_) {
        last_visible_ = ids;
        Intent intent{IntentKind::SetVisibleIds};
        intent.ids = ids;
        out.Push(std::move(intent));
    }
}

// @impl 0f87-6acd-2f33-0934
void MapViewController::Tick(const AppModel &model, Intents &out)
{
    ++frame_;
    view_model_.mode = model.selected >= 0 ? MapMode::Outing : MapMode::Overview;
    if (!fetcher_ || view_model_.width <= 0.0f)
        return;
    if (view_model_.mode == MapMode::Outing && model.points_id != view_model_.fitted_outing && !model.points.empty())
        FitOuting(model);
    if (view_model_.mode == MapMode::Overview && !view_model_.overview.fitted && !model.tracks_loading &&
        !model.tracks.empty())
        FitOverview(model);

    // Tiles arrived since the last frame; a missing one is asked again later.
    for (maptiles::ReadyTile &ready : fetcher_->TakeReady()) {
        if (ready.image.rgba.empty()) {
            retry_after_[ready.key] = NowS() + kRetrySeconds;
            continue;
        }
        MapTile &tile = view_model_.tiles[ready.key];
        tile.image = std::move(ready.image);
        tile.generation = ++generation_;
        tile.last_used = frame_;
    }
    RequestVisibleTiles();
    while (view_model_.tiles.size() > kMaxTiles) {  // least recently shown first
        auto oldest = view_model_.tiles.begin();
        for (auto it = view_model_.tiles.begin(); it != view_model_.tiles.end(); ++it)
            if (it->second.last_used < oldest->second.last_used)
                oldest = it;
        view_model_.tiles.erase(oldest);
    }
    view_model_.tile_errors = fetcher_->Errors();
    view_model_.tiles_pending = fetcher_->Pending();
    // Report an unavailable service once, until it comes back.
    if (fetcher_->Unavailable() && !unavailable_reported_) {
        unavailable_reported_ = true;
        out.PushText(IntentKind::ReportError,
                     "Map layer " + view_model_.Layer()->name + " is unavailable: none of its servers respond.");
    } else if (!fetcher_->Unavailable()) {
        unavailable_reported_ = false;
    }
    if (view_model_.mode == MapMode::Overview)
        PublishVisible(model, out);
}

// Overview: the shown track nearest the mouse, within kHoverPx. Distances
// are compared squared, in world pixels at the view zoom.
void MapViewController::UpdateHover(const AppModel &model, double mouse_x, double mouse_y)
{
    view_model_.hovered_track.clear();
    view_model_.hovered_group = -1;
    view_model_.hovered_segment = -1;
    const MapLayerInfo *layer = view_model_.Layer();
    if (!layer)
        return;
    if (model.show_segments) {
        UpdateSegmentHover(model, mouse_x, mouse_y);
        return;
    }
    std::set<std::string> shown;
    for (size_t i : core::SelectOutings(model.outings, model.MapFilter(), core::SortKey::Date, true))
        shown.insert(model.outings[i].raw.id);
    double left, top;
    MapOrigin(view_model_, left, top);
    const int zoom = view_model_.Camera().zoom;
    const double mouse_world_x = left + view_model_.width / 2.0 + mouse_x;
    const double mouse_world_y = top + view_model_.height / 2.0 + mouse_y;
    const maptiles::GeoBox box = MapViewBox(view_model_);
    double nearest = kHoverPx * kHoverPx;
    for (const core::OverviewTrack &track : model.tracks) {
        if (!shown.count(track.id) || track.east < box.west || track.west > box.east || track.north < box.south ||
            track.south > box.north)
            continue;
        for (size_t i = 0; i < track.lat.size(); ++i) {
            double x, y;
            maptiles::LonlatToWorld(track.lon[i], track.lat[i], zoom, layer->tile_size, x, y);
            const double distance2 = (x - mouse_world_x) * (x - mouse_world_x) + (y - mouse_world_y) * (y - mouse_world_y);
            if (distance2 < nearest) {
                nearest = distance2;
                view_model_.hovered_track = track.id;
            }
        }
    }
}

// Overview with segments: the segment nearest the mouse, within kHoverPx,
// measured to the lines between its points (they can be far apart on a
// straight segment). Shared segments win over single-route ones at equal
// distance, being drawn on top.
void MapViewController::UpdateSegmentHover(const AppModel &model, double mouse_x, double mouse_y)
{
    const MapLayerInfo *layer = view_model_.Layer();
    double left, top;
    MapOrigin(view_model_, left, top);
    const int zoom = view_model_.Camera().zoom;
    const double mouse_world_x = left + view_model_.width / 2.0 + mouse_x;
    const double mouse_world_y = top + view_model_.height / 2.0 + mouse_y;
    double nearest = kHoverPx * kHoverPx;
    bool nearest_shared = false;
    for (size_t g = 0; g < model.segment_groups.size(); ++g)
        for (const core::NetworkSegment &segment : model.segment_groups[g].network.segments) {
            double previous_x = 0.0, previous_y = 0.0;
            for (size_t i = 0; i < segment.lat.size(); ++i) {
                double x, y;
                maptiles::LonlatToWorld(segment.lon[i], segment.lat[i], zoom, layer->tile_size, x, y);
                if (i > 0) {
                    const double distance2 =
                        Distance2ToSegment(mouse_world_x, mouse_world_y, previous_x, previous_y, x, y);
                    const bool better = distance2 < nearest || (distance2 <= nearest && segment.Shared() && !nearest_shared);
                    if (better) {
                        nearest = distance2;
                        nearest_shared = segment.Shared();
                        view_model_.hovered_group = static_cast<int>(g);
                        view_model_.hovered_segment = segment.id;
                    }
                }
                previous_x = x;
                previous_y = y;
            }
        }
}

void MapViewController::Handle(const Intent &intent, const AppModel &model, Intents &out)
{
    MapCamera &camera = view_model_.mode == MapMode::Overview ? view_model_.overview : view_model_.outing;
    const MapLayerInfo *layer = view_model_.Layer();
    switch (intent.kind) {
    case IntentKind::MapViewport:
        view_model_.width = static_cast<float>(intent.x);
        view_model_.height = static_cast<float>(intent.y);
        break;
    case IntentKind::MapPan:
        if (layer) {
            double x, y;
            maptiles::LonlatToWorld(camera.lon, camera.lat, camera.zoom, layer->tile_size, x, y);
            maptiles::WorldToLonlat(x - intent.x, y - intent.y, camera.zoom, layer->tile_size, camera.lon,
                                    camera.lat);
        }
        break;
    case IntentKind::MapZoom:
        if (layer) {
            const int zoom = std::clamp(camera.zoom + intent.i, layer->min_zoom, MaxViewZoom(layer));
            if (zoom != camera.zoom) {
                // Keep the point under the mouse fixed: scale the mouse's
                // world position to the new zoom and put the centre back at
                // the same offset from it.
                double centre_x, centre_y;
                maptiles::LonlatToWorld(camera.lon, camera.lat, camera.zoom, layer->tile_size, centre_x, centre_y);
                const double scale = std::pow(2.0, zoom - camera.zoom);
                const double mouse_x = centre_x + intent.x, mouse_y = centre_y + intent.y;
                maptiles::WorldToLonlat(mouse_x * scale - intent.x, mouse_y * scale - intent.y, zoom,
                                        layer->tile_size, camera.lon, camera.lat);
                camera.zoom = zoom;
            }
        }
        break;
    case IntentKind::MapMouse:
        if (view_model_.mode == MapMode::Overview && intent.i)
            UpdateHover(model, intent.x, intent.y);
        else {
            view_model_.hovered_track.clear();
            view_model_.hovered_group = -1;
            view_model_.hovered_segment = -1;
        }
        break;
    case IntentKind::MapClick:
        if (view_model_.mode == MapMode::Overview && !view_model_.hovered_track.empty())
            for (size_t i = 0; i < model.outings.size(); ++i)
                if (model.outings[i].raw.id == view_model_.hovered_track)
                    out.Push(IntentKind::SelectOuting, static_cast<int>(i));
        break;
    case IntentKind::SelectLayer: {
        const std::string error = Select(intent.i);
        if (!error.empty())
            out.PushText(IntentKind::ReportError, "Cannot open the map layer: " + error);
        break;
    }
    case IntentKind::SetTrackStyle:
        view_model_.track_style = std::clamp(intent.i, 0, 5);
        break;
    case IntentKind::SetTrackColour:
        view_model_.track_colour = static_cast<unsigned int>(intent.x);
        break;
    default:
        break;
    }
}

} // namespace app
