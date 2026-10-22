// MapViewController with a fake tiles protocol whose URLs point nowhere
// (file:// paths that do not exist): no network, tiles simply fail.

#include <atf-c++.hpp>

#include "app/dispatch.h"
#include "app/map_geometry.h"
#include "app/map_view_utils.h"
#include "maptiles/mercator.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace {

struct FakeLayer {
    or_tile_layer info{};
};

void *FakeOpen(const char *json, char *, size_t)
{
    if (!json || std::string(json).find("\"fake\"") == std::string::npos)
        return nullptr;
    FakeLayer *l = new FakeLayer;
    l->info = {"fake", "Fake", "EPSG:3857", 1, 17, 256, {-180, -85.05, 180, 85.05}, "(c) fake", 1, ""};
    if (std::string(json).find("\"deep\"") != std::string::npos)
        l->info.max_zoom = 31;  // 1 << 31 overflows an int
    return l;
}
void FakeClose(void *h) { delete static_cast<FakeLayer *>(h); }
const or_tile_layer *FakeLayerInfo(void *h) { return &static_cast<FakeLayer *>(h)->info; }
int FakeUrl(void *, size_t, int z, int x, int y, char *url, size_t len)
{
    std::snprintf(url, len, "file:///nonexistent/openrider/%d/%d/%d.png", z, x, y);
    return 0;
}
const or_tiles_vtable kTiles = {sizeof(or_tiles_vtable), "fake", FakeOpen, FakeClose, FakeLayerInfo, FakeUrl,
                                nullptr, nullptr, nullptr, nullptr, nullptr};

// A layer published in Web Mercator and in Kartverket's UTM 33N grid
// (EPSG:25833, 19 levels, 21,664 m pixels at level 0 halving each level).
struct FakeGrids {
    or_tile_layer info{"grids", "Grids", "EPSG:3857", 0, 18, 256, {4, 57, 32, 72}, "(c) fake", 1, ""};
    std::vector<std::string> ids;
    std::vector<or_tile_matrix> mercator, utm;
    or_tile_grid grids[2];
};
void *GridsOpen(const char *json, char *, size_t)
{
    if (!json || std::string(json).find("\"grids\"") == std::string::npos)
        return nullptr;
    FakeGrids *g = new FakeGrids;
    g->ids.reserve(19);
    for (int level = 0; level <= 18; ++level) {
        g->ids.push_back(std::to_string(level));
        const int n = 1 << level;
        g->mercator.push_back({g->ids.back().c_str(), 2 * 20037508.342789244 / (256.0 * n), -20037508.342789244,
                               20037508.342789244, 256, 256, n, n, 0, n - 1, 0, n - 1});
        g->utm.push_back({g->ids.back().c_str(), 77371428.57142857 * 0.00028 / n, -2500000.0, 9045984.0, 256, 256, n, n, 0,
                          n - 1, 0, n - 1});
    }
    g->grids[0] = {"webmercator", "EPSG:3857", g->mercator.size(), g->mercator.data()};
    g->grids[1] = {"utm33n", "EPSG:25833", g->utm.size(), g->utm.data()};
    return g;
}
void GridsClose(void *h) { delete static_cast<FakeGrids *>(h); }
const or_tile_layer *GridsInfo(void *h) { return &static_cast<FakeGrids *>(h)->info; }
const char *GridsCapabilities(void *) { return nullptr; }
int GridsSetCapabilities(void *, const char *, size_t, char *, size_t) { return 0; }
size_t GridsCount(void *) { return 2; }
const or_tile_grid *GridsGrid(void *h, size_t index) { return &static_cast<FakeGrids *>(h)->grids[index]; }
int GridsTileUrl(void *, size_t, size_t grid, size_t level, int column, int row, char *url, size_t len)
{
    std::snprintf(url, len, "file:///nonexistent/openrider/%zu/%zu/%d/%d.png", grid, level, row, column);
    return 0;
}
const or_tiles_vtable kGridTiles = {sizeof(or_tiles_vtable), "grids", GridsOpen, GridsClose, GridsInfo, FakeUrl,
                                    GridsCapabilities, GridsSetCapabilities, GridsCount, GridsGrid, GridsTileUrl};

// SetMapCrs to `code`.
app::Intent CrsIntent(int code)
{
    app::Intent intent(app::IntentKind::SetMapCrs);
    intent.i = code;
    return intent;
}

const char *const kUtm33 = "EPSG:25833 | ETRS89 / UTM zone 33N | 12,34,18,84 | +proj=utm +zone=33 +ellps=GRS80 +units=m";

openrider::sdk::OverviewTrack LineTrack(const char *id, double lon0)
{
    openrider::sdk::OverviewTrack t;
    t.id = id;
    for (int i = 0; i <= 20; ++i) {
        t.lat.push_back(45.90 + i * 0.001);
        t.lon.push_back(lon0);
    }
    t.west = t.east = lon0;
    t.south = 45.90;
    t.north = 45.92;
    return t;
}

struct Fixture {
    app::AppModel model;
    app::MapViewController map;
    app::Intents out;

    Fixture()
    {
        char tmpl[] = "/tmp/openrider-mapvc-XXXXXX";
        map.AddPlugin(&kTiles);
        map.SetCacheDir(mkdtemp(tmpl));
        ATF_REQUIRE_EQ(map.AddLayer("{\"protocol\":\"fake\"}"), "");
        ATF_REQUIRE_EQ(map.Select(0), "");
        for (int i = 0; i < 2; ++i) {
            core::Outing o{};
            std::snprintf(o.raw.id, sizeof o.raw.id, "t%d", i);
            std::snprintf(o.raw.start_local, sizeof o.raw.start_local, "2026-01-0%d 10:00:00", i + 1);
            model.outings.push_back(o);
        }
        model.tracks = {LineTrack("t0", 6.60), LineTrack("t1", 6.70)};
        app::Intent vp{app::IntentKind::MapViewport};
        vp.x = 800;
        vp.y = 600;
        map.Handle(vp, model, out);
        map.Tick(model, out);  // overview: fits both tracks
    }
};

} // namespace

ATF_TEST_CASE_WITHOUT_HEAD(overview_fits_tracks_and_publishes_visible);
ATF_TEST_CASE_BODY(overview_fits_tracks_and_publishes_visible)
{
    Fixture f;
    ATF_REQUIRE(f.map.ViewModel().mode == app::MapMode::Overview);
    ATF_REQUIRE(f.map.ViewModel().overview.fitted);
    const maptiles::GeoBox b = app::MapViewBox(f.map.ViewModel());
    ATF_REQUIRE(b.west < 6.60 && b.east > 6.70 && b.south < 45.90 && b.north > 45.92);
    bool published = false;
    for (const app::Intent &in : f.out.Take())
        if (in.kind == app::IntentKind::SetVisibleIds)
            published = in.ids.size() == 2;
    ATF_REQUIRE(published);
}

ATF_TEST_CASE_WITHOUT_HEAD(pan_and_zoom_move_the_camera);
ATF_TEST_CASE_BODY(pan_and_zoom_move_the_camera)
{
    Fixture f;
    const app::MapCamera before = f.map.ViewModel().overview;
    app::Intent pan{app::IntentKind::MapPan};
    pan.x = 100;  // dragging right moves the view west
    f.map.Handle(pan, f.model, f.out);
    ATF_REQUIRE(f.map.ViewModel().overview.lon < before.lon);
    // Relative steps: a quarter level scales the map by 2^0.25.
    const double start = f.map.ViewModel().overview.zoom;
    app::Intent step{app::IntentKind::MapZoom};
    step.z = 0.25;
    f.map.Handle(step, f.model, f.out);
    ATF_REQUIRE_EQ(f.map.ViewModel().overview.zoom, start + 0.25);
    for (int i = 0; i < 100; ++i)
        f.map.Handle(step, f.model, f.out);
    ATF_REQUIRE_EQ(f.map.ViewModel().overview.zoom, static_cast<double>(app::kMaxViewZoom));  // over-zoom past 17, capped at 20
}

ATF_TEST_CASE_WITHOUT_HEAD(button_zoom_keeps_the_centre);
ATF_TEST_CASE_BODY(button_zoom_keeps_the_centre)
{
    Fixture f;
    const app::MapCamera before = f.map.ViewModel().overview;
    app::Intent step{app::IntentKind::MapZoom};  // a button: no mouse offset
    step.z = 0.5;
    f.map.Handle(step, f.model, f.out);
    const app::MapCamera &after = f.map.ViewModel().overview;
    ATF_REQUIRE_EQ(after.zoom, before.zoom + 0.5);
    ATF_REQUIRE(std::fabs(after.lon - before.lon) < 1e-9 && std::fabs(after.lat - before.lat) < 1e-9);
    step.z = -100.0;  // below the layer's first level
    f.map.Handle(step, f.model, f.out);
    ATF_REQUIRE_EQ(f.map.ViewModel().overview.zoom, 1.0);
}

ATF_TEST_CASE_WITHOUT_HEAD(tile_level_is_the_nearest_served);
ATF_TEST_CASE_BODY(tile_level_is_the_nearest_served)
{
    // A plain XYZ layer (levels 1..17): its Web Mercator grid, shown as is.
    Fixture f;
    const app::MapViewModel &vm = f.map.ViewModel();
    ATF_REQUIRE_EQ(vm.Layer()->grids.size(), 1u);
    ATF_REQUIRE_EQ(app::MaxViewZoom(vm.Layer()), app::kMaxViewZoom);
    for (const auto &[zoom, level] : std::vector<std::pair<double, int>>{{12.4, 12}, {12.6, 13}, {19.7, 17}}) {
        app::MapCamera camera = vm.overview;
        camera.zoom = zoom;
        const app::MapGeometry geometry(vm.display, camera, vm.width, vm.height);
        ATF_REQUIRE_EQ(app::SourceGrid(vm, geometry), 0);
        const std::vector<app::VisibleTile> tiles = app::VisibleTiles(vm, geometry, 0);
        ATF_REQUIRE(!tiles.empty());
        ATF_REQUIRE_EQ(tiles[0].key.z, level);  // over-zoom past 17: the last level, enlarged
    }
}

ATF_TEST_CASE_WITHOUT_HEAD(hover_and_click_select_an_outing);
ATF_TEST_CASE_BODY(hover_and_click_select_an_outing)
{
    Fixture f;
    f.out.Take();
    // The mouse over track t1: its world position relative to the centre.
    const app::MapViewModel &vm = f.map.ViewModel();
    double x, y;
    ATF_REQUIRE(app::MapGeometry(vm).ToScreen(6.70, 45.91, x, y));
    app::Intent mouse{app::IntentKind::MapMouse};
    mouse.i = 1;
    mouse.x = x - vm.width / 2.0;
    mouse.y = y - vm.height / 2.0;
    f.map.Handle(mouse, f.model, f.out);
    ATF_REQUIRE_EQ(f.map.ViewModel().hovered_track, "t1");
    f.map.Handle(app::Intent{app::IntentKind::MapClick}, f.model, f.out);
    const auto follow = f.out.Take();
    ATF_REQUIRE_EQ(follow.size(), 1u);
    ATF_REQUIRE(follow[0].kind == app::IntentKind::SelectOuting);
    ATF_REQUIRE_EQ(follow[0].i, 1);
}

ATF_TEST_CASE_WITHOUT_HEAD(outing_mode_fits_the_track);
ATF_TEST_CASE_BODY(outing_mode_fits_the_track)
{
    Fixture f;
    f.model.selected = 0;
    f.model.points_id = "t0";
    for (int i = 0; i <= 20; ++i) {
        or_point p{};
        p.lat = 45.90 + i * 0.0005;
        p.lon = 6.60;
        f.model.points.push_back(p);
    }
    f.map.Tick(f.model, f.out);
    ATF_REQUIRE(f.map.ViewModel().mode == app::MapMode::Outing);
    ATF_REQUIRE_EQ(f.map.ViewModel().fitted_outing, "t0");
    ATF_REQUIRE(f.map.ViewModel().outing.zoom > f.map.ViewModel().overview.zoom);  // a single short track
    ATF_REQUIRE(f.map.ViewModel().overview.fitted);                          // overview camera kept
}

ATF_TEST_CASE_WITHOUT_HEAD(layer_with_too_deep_zoom_refused);
ATF_TEST_CASE_BODY(layer_with_too_deep_zoom_refused)
{
    app::MapViewController map;
    map.AddPlugin(&kTiles);
    ATF_REQUIRE(map.AddLayer("{\"protocol\":\"fake\",\"deep\":1}").find("zoom levels") != std::string::npos);
    ATF_REQUIRE(map.ViewModel().layers.empty());
    ATF_REQUIRE_EQ(map.AddLayer("{\"protocol\":\"other\"}"), "not usable by any tiles plugin");
}

ATF_TEST_CASE_WITHOUT_HEAD(local_grid_feeds_the_projections);
ATF_TEST_CASE_BODY(local_grid_feeds_the_projections)
{
    // Over Svolvær, the layer's UTM 33N tiles are drawn, warped, in the
    // default Web Mercator; the projection choices are its grids' systems.
    app::MapViewController map;
    app::AppModel model;
    app::Intents out;
    char tmpl[] = "/tmp/openrider-mapvc-XXXXXX";
    map.AddPlugin(&kGridTiles);
    map.SetCacheDir(mkdtemp(tmpl));
    ATF_REQUIRE_EQ(map.AddCrs(kUtm33), "");
    ATF_REQUIRE_EQ(map.AddLayer("{\"protocol\":\"grids\"}"), "");
    ATF_REQUIRE_EQ(map.Select(0), "");
    const app::MapViewModel &vm = map.ViewModel();
    ATF_REQUIRE_EQ(vm.display.epsg, 3857);
    ATF_REQUIRE_EQ(vm.display_choices.size(), 2u);
    ATF_REQUIRE_EQ(vm.display_choices[1].epsg, 25833);
    app::Intent viewport{app::IntentKind::MapViewport};
    viewport.x = 800;
    viewport.y = 600;
    map.Handle(viewport, model, out);
    model.tracks = {LineTrack("t0", 14.5684)};
    model.tracks[0].lat = {68.2346, 68.2446};
    model.tracks[0].lon = {14.5684, 14.5784};
    model.tracks[0].south = 68.2346;
    model.tracks[0].north = 68.2446;
    model.tracks[0].west = 14.5684;
    model.tracks[0].east = 14.5784;
    map.Tick(model, out);  // fits the overview over Svolvær
    const app::MapGeometry mercator(vm);
    ATF_REQUIRE_EQ(app::SourceGrid(vm, mercator), 1);  // the local grid
    const std::vector<app::VisibleTile> warped = app::VisibleTiles(vm, mercator, 1);
    ATF_REQUIRE(!warped.empty());
    // Switching to UTM 33N keeps the ground scale: at 68 N, Web Mercator
    // units are 1/cos(68) = 2.67 times smaller than ground metres, so the
    // same scale in UTM's true metres is 1.4 zoom levels higher.
    const double mercator_zoom = vm.overview.zoom;
    map.Handle(CrsIntent(25833), model, out);
    ATF_REQUIRE_EQ(vm.display.epsg, 25833);
    ATF_REQUIRE(std::fabs(vm.overview.zoom - mercator_zoom - std::log2(1.0 / std::cos(68.24 * 3.14159265 / 180))) < 0.02);
    const app::MapGeometry utm(vm);
    ATF_REQUIRE_EQ(app::SourceGrid(vm, utm), 1);  // native now
    const std::vector<app::VisibleTile> native = app::VisibleTiles(vm, utm, 1);
    ATF_REQUIRE(!native.empty());
    // Native tiles match the screen's pixels: about one tile pixel each.
    const maptiles::TileMatrix &level = vm.Layer()->grids[1].tiles.levels[static_cast<size_t>(native[0].key.z)];
    ATF_REQUIRE(std::fabs(std::log2(level.resolution / utm.Resolution())) <= 0.5);
    // The track lies where its UTM coordinates say, on screen.
    double x, y;
    ATF_REQUIRE(utm.ToScreen(14.5684, 68.2346, x, y));
    double sx, sy;
    utm.DisplayToScreen(482140.0044, 7569082.9568, sx, sy);  // PROJ's UTM 33N of the same point
    ATF_REQUIRE(std::hypot(x - sx, y - sy) < 0.2);
    // A system the layer does not publish is not offered.
    map.Handle(CrsIntent(2154), model, out);
    ATF_REQUIRE_EQ(vm.display.epsg, 25833);
}

ATF_TEST_CASE_WITHOUT_HEAD(national_system_pan_and_layer_change);
ATF_TEST_CASE_BODY(national_system_pan_and_layer_change)
{
    app::MapViewController map;
    app::AppModel model;
    app::Intents out;
    char tmpl[] = "/tmp/openrider-mapvc-XXXXXX";
    map.AddPlugin(&kGridTiles);
    map.AddPlugin(&kTiles);
    map.SetCacheDir(mkdtemp(tmpl));
    ATF_REQUIRE_EQ(map.AddCrs(kUtm33), "");
    ATF_REQUIRE_EQ(map.AddLayer("{\"protocol\":\"grids\"}"), "");
    ATF_REQUIRE_EQ(map.AddLayer("{\"protocol\":\"fake\"}"), "");
    ATF_REQUIRE_EQ(map.Select(0), "");
    app::Intent viewport{app::IntentKind::MapViewport};
    viewport.x = 800;
    viewport.y = 600;
    map.Handle(viewport, model, out);
    map.Handle(CrsIntent(25833), model, out);
    const app::MapViewModel &vm = map.ViewModel();
    ATF_REQUIRE_EQ(vm.display.epsg, 25833);
    // Dragging 100 px right brings the point 100 px left of the centre to it.
    double lon, lat;
    ATF_REQUIRE(app::MapGeometry(vm).ToLonlat(300, 300, lon, lat));
    app::Intent pan{app::IntentKind::MapPan};
    pan.x = 100;
    map.Handle(pan, model, out);
    ATF_REQUIRE(std::fabs(vm.overview.lon - lon) < 1e-9 && std::fabs(vm.overview.lat - lat) < 1e-9);
    // A layer without that system: back to Web Mercator, at the same scale.
    const double utm_zoom = vm.overview.zoom;
    map.Handle(app::Intent(app::IntentKind::SelectLayer), model, out);  // index 0: already shown, nothing
    app::Intent select{app::IntentKind::SelectLayer};
    select.i = 1;
    map.Handle(select, model, out);
    ATF_REQUIRE_EQ(vm.display.epsg, 3857);
    ATF_REQUIRE_EQ(vm.display_choices.size(), 1u);
    ATF_REQUIRE(vm.overview.zoom != utm_zoom);
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, national_system_pan_and_layer_change);
    ATF_ADD_TEST_CASE(tcs, local_grid_feeds_the_projections);
    ATF_ADD_TEST_CASE(tcs, overview_fits_tracks_and_publishes_visible);
    ATF_ADD_TEST_CASE(tcs, layer_with_too_deep_zoom_refused);
    ATF_ADD_TEST_CASE(tcs, pan_and_zoom_move_the_camera);
    ATF_ADD_TEST_CASE(tcs, button_zoom_keeps_the_centre);
    ATF_ADD_TEST_CASE(tcs, tile_level_is_the_nearest_served);
    ATF_ADD_TEST_CASE(tcs, hover_and_click_select_an_outing);
    ATF_ADD_TEST_CASE(tcs, outing_mode_fits_the_track);
}
