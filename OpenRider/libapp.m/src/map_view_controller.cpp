#include "app/map_view_controller.h"

#include "app/map_geometry.h"
#include "app/map_view_utils.h"

#include "core/log.h"
#include "core/outing_selection.h"
#include "core/plugin_thread.h"
#include "core/plugins.h"
#include "maptiles/capabilities.h"

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

// Centres `camera` on the box and picks the largest whole zoom showing it
// within 90% of the viewport, leaving two levels of zoom-in headroom.
void FitCamera(MapCamera &camera, const MapLayerInfo &layer, const reproj::Crs &display, double west, double south,
               double east, double north, float width, float height)
{
    // The box in the display system: its border, projected.
    const reproj::Transformer to_display(reproj::CrsRegistry().Find(4326), display);
    double x0 = 1e300, y0 = 1e300, x1 = -1e300, y1 = -1e300;
    constexpr int kSteps = 8;
    for (int i = 0; i <= kSteps; ++i)
        for (int j = 0; j <= kSteps; ++j) {
            double x, y;
            if (!to_display.Transform(west + (east - west) * i / kSteps, south + (north - south) * j / kSteps, x, y))
                continue;
            x0 = std::min(x0, x);
            x1 = std::max(x1, x);
            y0 = std::min(y0, y);
            y1 = std::max(y1, y);
        }
    if (x0 > x1)
        return;
    const double resolution = std::max((x1 - x0) / (width * 0.9), (y1 - y0) / (height * 0.9));
    const double fit = resolution > 0.0 ? std::floor(ZoomAtResolution(display, resolution)) : kMaxViewZoom;
    camera.zoom = std::clamp(fit, static_cast<double>(layer.min_zoom),
                             static_cast<double>(std::max(layer.max_zoom, kMaxViewZoom - 2)));
    double lon, lat;
    if (to_display.Inverse().Transform((x0 + x1) / 2.0, (y0 + y1) / 2.0, lon, lat)) {
        camera.lon = lon;
        camera.lat = lat;
    }
    camera.fitted = true;
}

} // namespace

MapViewController::MapViewController()
{
    view_model_.display = registry_.Find(3857);
    view_model_.display_choices = {view_model_.display};
}

std::vector<std::string> MapViewController::LoadCrs(const std::string &path)
{
    return registry_.LoadFile(path);
}

std::string MapViewController::AddCrs(const std::string &line)
{
    std::string error;
    return registry_.Add(line, error) ? "" : error;
}

// @impl 0f87-6ada-53d7-171a
void MapViewController::SetDisplay(const reproj::Crs &crs)
{
    if (!crs.Valid() || crs.epsg == view_model_.display.epsg)
        return;
    // Each camera keeps its ground scale at its centre: the resolution
    // follows the ratio of the two systems' scale factors there.
    for (MapCamera *camera : {&view_model_.overview, &view_model_.outing, &view_model_.comparison}) {
        const double old_scale = reproj::ScaleFactor(view_model_.display, camera->lon, camera->lat);
        const double new_scale = reproj::ScaleFactor(crs, camera->lon, camera->lat);
        const double resolution = ResolutionAtZoom(view_model_.display, camera->zoom) * new_scale / old_scale;
        camera->zoom = ZoomAtResolution(crs, resolution);
    }
    view_model_.display = crs;
    core::LogLine("map shown in %s (%s)", crs.Id().c_str(), crs.name.c_str());
}

MapViewController::~MapViewController() { Shutdown(); }

std::string MapViewController::AddLayer(const std::string &json)
{
    MapLayerInfo info;
    info.json = json;
    // The first protocol that opens the definition describes the layer.
    for (const or_tiles_vtable *tiles_vtable : plugins_) {
        char error[256] = "";
        void *handle = nullptr;
        core::CallPlugin(tiles_vtable, [&] { handle = tiles_vtable->open(info.json.c_str(), error, sizeof error); });
        if (!handle)
            continue;
        const or_tile_layer *layer = nullptr;
        core::CallPlugin(tiles_vtable, [&] { layer = tiles_vtable->layer(handle); });
        info.id = layer->id;
        info.name = layer->name;
        info.attribution = layer->attribution ? layer->attribution : "";
        for (int k = 0; k < 4; ++k)
            info.extent[k] = layer->extent[k];
        info.min_zoom = layer->min_zoom;
        info.max_zoom = layer->max_zoom;
        info.tile_size = layer->tile_size;
        core::CallPlugin(tiles_vtable, [&] { tiles_vtable->close(handle); });
        if (info.min_zoom < 0 || info.max_zoom > kMaxTileZoom || info.min_zoom > info.max_zoom || info.tile_size <= 0)
            return "zoom levels " + std::to_string(info.min_zoom) + ".." + std::to_string(info.max_zoom) +
                   " outside 0.." + std::to_string(kMaxTileZoom) + ", or no tile size";
        view_model_.layers.push_back(info);
        return "";
    }
    return "not usable by any tiles plugin";
}

void MapViewController::Shutdown()
{
    fetcher_.reset();  // joins the workers before the layer goes away
    cache_.reset();
    if (tiles_vtable_ && layer_)
        core::CallPlugin(tiles_vtable_, [&] { tiles_vtable_->close(layer_); });
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
            if (const std::string why = AddLayer(ReadText(path)); !why.empty())
                problems.push_back("map layer " + path + ": " + why);
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
    const MapLayerInfo info = view_model_.layers[static_cast<size_t>(index)];
    std::string why;
    for (const or_tiles_vtable *tiles_vtable : plugins_) {
        char error[256] = "";
        void *handle = nullptr;
        core::CallPlugin(tiles_vtable, [&] { handle = tiles_vtable->open(info.json.c_str(), error, sizeof error); });
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
    // The grids: a WMTS layer learns them from the service's capabilities.
    const std::string user_agent = "OpenRider/0.1 (desktop outing viewer; libcurl)";
    if (const std::string error = maptiles::ProvideCapabilities(tiles_vtable_, layer_, cache_dir_, user_agent);
        !error.empty()) {
        core::CallPlugin(tiles_vtable_, [&] { tiles_vtable_->close(layer_); });
        layer_ = nullptr;
        return error;
    }
    std::vector<maptiles::TileGrid> tile_grids = maptiles::GridsOf(tiles_vtable_, layer_);
    MapLayerInfo &opened = view_model_.layers[static_cast<size_t>(index)];
    opened.grids.clear();
    view_model_.display_choices = {registry_.Find(3857)};
    for (const maptiles::TileGrid &grid : tile_grids) {
        MapGrid map_grid{grid, registry_.Find(grid.crs)};
        if (!map_grid.crs.Valid())
            core::LogLine("map layer %s: grid %s in %s, an unknown coordinate system", opened.name.c_str(),
                          grid.id.c_str(), grid.crs.c_str());
        else if (std::none_of(view_model_.display_choices.begin(), view_model_.display_choices.end(),
                              [&](const reproj::Crs &crs) { return crs.epsg == map_grid.crs.epsg; }))
            view_model_.display_choices.push_back(map_grid.crs);
        opened.grids.push_back(std::move(map_grid));
    }
    if (std::none_of(view_model_.display_choices.begin(), view_model_.display_choices.end(),
                     [&](const reproj::Crs &crs) { return crs.epsg == view_model_.display.epsg; }))
        SetDisplay(registry_.Find(3857));
    cache_ = std::make_unique<maptiles::TileCache>(cache_dir_, maptiles::kDefaultCacheBytes);
    fetcher_ = std::make_unique<maptiles::TileFetcher>(tiles_vtable_, layer_, std::move(tile_grids), cache_.get(),
                                                       user_agent);
    view_model_.current = index;
    view_model_.tiles.clear();
    retry_after_.clear();
    unavailable_reported_ = false;
    for (MapCamera *camera : {&view_model_.overview, &view_model_.outing})
        camera->zoom = std::clamp(camera->zoom, static_cast<double>(info.min_zoom), static_cast<double>(MaxViewZoom(&info)));
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
        FitCamera(view_model_.outing, *view_model_.Layer(), view_model_.display, west, south, east, north, view_model_.width,
                  view_model_.height);
    view_model_.fitted_outing = model.points_id;
}

void MapViewController::FitComparison(const AppModel &model)
{
    double west = 180, east = -180, south = 90, north = -90;
    auto extend = [&](double lon, double lat) {
        if (std::isnan(lon) || std::isnan(lat))
            return;
        west = std::min(west, lon);
        east = std::max(east, lon);
        south = std::min(south, lat);
        north = std::max(north, lat);
    };
    for (size_t i = 0; i < model.comparison.segment.lat.size(); ++i)
        extend(model.comparison.segment.lon[i], model.comparison.segment.lat[i]);
    for (const ComparedRun &run : model.comparison.runs)
        for (const or_point &point : run.points)
            extend(point.lon, point.lat);
    if (west <= east && view_model_.Layer())
        FitCamera(view_model_.comparison, *view_model_.Layer(), view_model_.display, west, south, east, north, view_model_.width,
                  view_model_.height);
    view_model_.fitted_comparison = model.comparison.generation;
}

void MapViewController::FitOverview(const AppModel &model)
{
    double west = 180, east = -180, south = 90, north = -90;
    for (const openrider::sdk::OverviewTrack &track : model.tracks) {
        west = std::min(west, track.west);
        east = std::max(east, track.east);
        south = std::min(south, track.south);
        north = std::max(north, track.north);
    }
    if (west <= east && view_model_.Layer())
        FitCamera(view_model_.overview, *view_model_.Layer(), view_model_.display, west, south, east, north, view_model_.width,
                  view_model_.height);
}

// Requests the tiles covering the view, from the grid SourceGrid() picks.
// @impl 0f87-6acd-5daf-1652
void MapViewController::RequestVisibleTiles()
{
    if (!view_model_.Layer() || !fetcher_ || view_model_.width <= 0.0f)
        return;
    const MapGeometry geometry(view_model_);
    const double now = NowS();
    for (const VisibleTile &tile : VisibleTiles(view_model_, geometry, SourceGrid(view_model_, geometry))) {
        auto decoded = view_model_.tiles.find(tile.key);
        if (decoded != view_model_.tiles.end()) {
            decoded->second.last_used = frame_;
            continue;
        }
        auto retry = retry_after_.find(tile.key);
        if (retry == retry_after_.end() || retry->second <= now)
            fetcher_->Request(tile.key);
    }
}

void MapViewController::PublishVisible(const AppModel &model, Intents &out)
{
    const maptiles::GeoBox box = MapViewBox(view_model_);
    std::vector<std::string> ids;
    for (const openrider::sdk::OverviewTrack &track : model.tracks)
        if (openrider::sdk::TrackInBox(track, box.west, box.south, box.east, box.north))
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
    view_model_.mode = model.comparison.open ? MapMode::Comparison
                       : model.selected >= 0  ? MapMode::Outing
                                              : MapMode::Overview;
    if (!fetcher_ || view_model_.width <= 0.0f)
        return;
    if (view_model_.mode == MapMode::Outing && model.points_id != view_model_.fitted_outing && !model.points.empty())
        FitOuting(model);
    if (view_model_.mode == MapMode::Comparison && model.comparison.generation != view_model_.fitted_comparison)
        FitComparison(model);
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
    const MapGeometry geometry(view_model_);
    const double mouse_screen_x = view_model_.width / 2.0 + mouse_x;
    const double mouse_screen_y = view_model_.height / 2.0 + mouse_y;
    const maptiles::GeoBox &box = geometry.ViewBox();
    double nearest = kHoverPx * kHoverPx;
    for (const openrider::sdk::OverviewTrack &track : model.tracks) {
        if (!shown.count(track.id) || track.east < box.west || track.west > box.east || track.north < box.south ||
            track.south > box.north)
            continue;
        for (size_t i = 0; i < track.lat.size(); ++i) {
            double x, y;
            if (!geometry.ToScreen(track.lon[i], track.lat[i], x, y))
                continue;
            const double distance2 = (x - mouse_screen_x) * (x - mouse_screen_x) + (y - mouse_screen_y) * (y - mouse_screen_y);
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
    const MapGeometry geometry(view_model_);
    const double mouse_screen_x = view_model_.width / 2.0 + mouse_x;
    const double mouse_screen_y = view_model_.height / 2.0 + mouse_y;
    double nearest = kHoverPx * kHoverPx;
    bool nearest_shared = false;
    for (size_t g = 0; g < model.segment_groups.size(); ++g)
        for (const openrider::sdk::NetworkSegment &segment : model.segment_groups[g].network.segments) {
            double previous_x = 0.0, previous_y = 0.0;
            bool previous = false;
            for (size_t i = 0; i < segment.lat.size(); ++i) {
                double x, y;
                if (!geometry.ToScreen(segment.lon[i], segment.lat[i], x, y)) {
                    previous = false;
                    continue;
                }
                if (previous) {
                    const double distance2 =
                        Distance2ToSegment(mouse_screen_x, mouse_screen_y, previous_x, previous_y, x, y);
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
                previous = true;
            }
        }
}

void MapViewController::Handle(const Intent &intent, const AppModel &model, Intents &out)
{
    MapCamera &camera = view_model_.mode == MapMode::Overview ? view_model_.overview
                        : view_model_.mode == MapMode::Outing ? view_model_.outing
                                                              : view_model_.comparison;
    const MapLayerInfo *layer = view_model_.Layer();
    switch (intent.kind) {
    case IntentKind::MapViewport:
        view_model_.width = static_cast<float>(intent.x);
        view_model_.height = static_cast<float>(intent.y);
        break;
    case IntentKind::MapPan:
        if (layer) {
            // The new centre: the point now `delta` pixels from it.
            const MapGeometry geometry(view_model_);
            double lon, lat;
            if (geometry.ToLonlat(view_model_.width / 2.0 - intent.x, view_model_.height / 2.0 - intent.y, lon, lat)) {
                camera.lon = lon;
                camera.lat = lat;
            }
        }
        break;
    // @impl 0f87-6ad8-552c-4753
    case IntentKind::MapZoom:
        if (layer) {
            // Relative steps: zooming by z levels scales the map by 2^z,
            // whatever the latitude and the current level.
            const double zoom = std::clamp(camera.zoom + intent.z, static_cast<double>(layer->min_zoom),
                                           static_cast<double>(MaxViewZoom(layer)));
            if (zoom != camera.zoom) {
                // Keep the point under the mouse fixed.
                const MapGeometry geometry(view_model_);
                camera = geometry.ZoomedAt(camera, zoom, view_model_.width / 2.0 + intent.x,
                                           view_model_.height / 2.0 + intent.y);
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
    case IntentKind::MapDoubleClick:
        // Overview with segments: double-clicking a segment compares the
        // outings along it.
        if (view_model_.mode == MapMode::Overview && model.show_segments && view_model_.hovered_segment >= 0)
            out.Push(IntentKind::OpenSegmentComparison, view_model_.hovered_group, view_model_.hovered_segment);
        break;
    case IntentKind::SelectLayer: {
        const std::string error = Select(intent.i);
        if (!error.empty())
            out.PushText(IntentKind::ReportError, "Cannot open the map layer: " + error);
        break;
    }
    case IntentKind::SetMapCrs:
        for (const reproj::Crs &crs : view_model_.display_choices)
            if (crs.epsg == intent.i)
                SetDisplay(crs);
        break;
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
