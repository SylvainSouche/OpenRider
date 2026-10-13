// MapViewController with a fake tiles protocol whose URLs point nowhere
// (file:// paths that do not exist): no network, tiles simply fail.

#include <atf-c++.hpp>

#include "app/dispatch.h"
#include "app/map_view_utils.h"
#include "maptiles/mercator.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

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
    return l;
}
void FakeClose(void *h) { delete static_cast<FakeLayer *>(h); }
const or_tile_layer *FakeLayerInfo(void *h) { return &static_cast<FakeLayer *>(h)->info; }
int FakeUrl(void *, size_t, int z, int x, int y, char *url, size_t len)
{
    std::snprintf(url, len, "file:///nonexistent/openrider/%d/%d/%d.png", z, x, y);
    return 0;
}
const or_tiles_vtable kTiles = {"fake", FakeOpen, FakeClose, FakeLayerInfo, FakeUrl};

core::OverviewTrack LineTrack(const char *id, double lon0)
{
    core::OverviewTrack t;
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
        ATF_REQUIRE(map.AddLayer("{\"protocol\":\"fake\"}"));
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
    for (int i = 0; i < 30; ++i) {
        app::Intent zoom{app::IntentKind::MapZoom};
        zoom.i = 1;
        f.map.Handle(zoom, f.model, f.out);
    }
    ATF_REQUIRE_EQ(f.map.ViewModel().overview.zoom, app::kMaxViewZoom);  // over-zoom past 17, capped at 20
}

ATF_TEST_CASE_WITHOUT_HEAD(hover_and_click_select_an_outing);
ATF_TEST_CASE_BODY(hover_and_click_select_an_outing)
{
    Fixture f;
    f.out.Take();
    // The mouse over track t1: its world position relative to the centre.
    const app::MapViewModel &vm = f.map.ViewModel();
    double left, top, x, y;
    app::MapOrigin(vm, left, top);
    maptiles::LonlatToWorld(6.70, 45.91, vm.overview.zoom, 256, x, y);
    app::Intent mouse{app::IntentKind::MapMouse};
    mouse.i = 1;
    mouse.x = x - left - vm.width / 2.0;
    mouse.y = y - top - vm.height / 2.0;
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

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, overview_fits_tracks_and_publishes_visible);
    ATF_ADD_TEST_CASE(tcs, pan_and_zoom_move_the_camera);
    ATF_ADD_TEST_CASE(tcs, hover_and_click_select_an_outing);
    ATF_ADD_TEST_CASE(tcs, outing_mode_fits_the_track);
}
