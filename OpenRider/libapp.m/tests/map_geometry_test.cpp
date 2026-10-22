// The map's geometry and the choice of tiles, in Web Mercator and in
// national systems.

#include <atf-c++.hpp>

#include "app/map_geometry.h"
#include "app/map_view_utils.h"

#include <cmath>

namespace {

reproj::CrsRegistry Registry()
{
    reproj::CrsRegistry registry;
    registry.Parse("EPSG:25833 | ETRS89 / UTM zone 33N | 12,34,18,84 | +proj=utm +zone=33 +ellps=GRS80 +units=m\n"
                   "EPSG:2154 | RGF93 v1 / Lambert-93 | -9.86,41.15,10.38,51.56 | +proj=lcc +lat_0=46.5 +lon_0=3 "
                   "+lat_1=49 +lat_2=44 +x_0=700000 +y_0=6600000 +ellps=GRS80 +units=m\n");
    return registry;
}

app::MapCamera Camera(double lon, double lat, double zoom)
{
    app::MapCamera camera;
    camera.lon = lon;
    camera.lat = lat;
    camera.zoom = zoom;
    return camera;
}

// A grid of `levels` power-of-two levels in `crs`, origin (x0, y0), level
// 0's pixel `resolution`.
app::MapGrid Grid(const reproj::Crs &crs, double x0, double y0, double resolution, int levels)
{
    app::MapGrid grid{maptiles::TileGrid{}, crs};
    grid.tiles.crs = crs.Id();
    for (int l = 0; l < levels; ++l) {
        maptiles::TileMatrix m;
        m.id = std::to_string(l);
        m.resolution = resolution / (1 << l);
        m.origin_x = x0;
        m.origin_y = y0;
        m.matrix_width = m.matrix_height = 1 << l;
        m.last_column = m.last_row = (1 << l) - 1;
        grid.tiles.levels.push_back(m);
    }
    return grid;
}

} // namespace

ATF_TEST_CASE_WITHOUT_HEAD(screen_round_trip);
ATF_TEST_CASE_BODY(screen_round_trip)
{
    const reproj::CrsRegistry registry = Registry();
    for (int code : {3857, 25833, 2154}) {
        const app::MapGeometry geometry(registry.Find(code), Camera(code == 2154 ? 6.6 : 14.57, code == 2154 ? 45.9 : 68.23, 12.5),
                                        800, 600);
        ATF_REQUIRE(geometry.Valid());
        // The camera's centre is the screen's.
        double x, y;
        ATF_REQUIRE(geometry.ToScreen(code == 2154 ? 6.6 : 14.57, code == 2154 ? 45.9 : 68.23, x, y));
        ATF_REQUIRE(std::fabs(x - 400.0) < 0.3 && std::fabs(y - 300.0) < 0.3);
        // Any screen point and back, to a quarter of a pixel.
        for (double sx : {0.0, 123.0, 800.0})
            for (double sy : {0.0, 456.0, 600.0}) {
                double lon, lat, bx, by;
                ATF_REQUIRE(geometry.ToLonlat(sx, sy, lon, lat));
                ATF_REQUIRE(geometry.ToScreen(lon, lat, bx, by));
                ATF_REQUIRE(std::hypot(bx - sx, by - sy) < 0.3);
            }
        // North is up, east is right.
        double north_x, north_y, east_x, east_y;
        ATF_REQUIRE(geometry.ToLonlat(400, 0, north_x, north_y));
        ATF_REQUIRE(geometry.ToLonlat(800, 300, east_x, east_y));
        ATF_REQUIRE(north_y > (code == 2154 ? 45.9 : 68.23));
        ATF_REQUIRE(east_x > (code == 2154 ? 6.6 : 14.57));
        // The view box holds the view.
        const maptiles::GeoBox &box = geometry.ViewBox();
        ATF_REQUIRE(box.west < east_x && box.east >= east_x && box.north >= north_y);
    }
}

ATF_TEST_CASE_WITHOUT_HEAD(zoom_keeps_the_point_under_the_mouse);
ATF_TEST_CASE_BODY(zoom_keeps_the_point_under_the_mouse)
{
    const reproj::CrsRegistry registry = Registry();
    const app::MapCamera camera = Camera(14.57, 68.23, 11.0);
    const app::MapGeometry before(registry.Find(25833), camera, 800, 600);
    double lon, lat;
    ATF_REQUIRE(before.ToLonlat(650, 120, lon, lat));
    const app::MapCamera zoomed = before.ZoomedAt(camera, 12.75, 650, 120);
    ATF_REQUIRE_EQ(zoomed.zoom, 12.75);
    const app::MapGeometry after(registry.Find(25833), zoomed, 800, 600);
    double x, y;
    ATF_REQUIRE(after.ToScreen(lon, lat, x, y));
    ATF_REQUIRE(std::fabs(x - 650) < 0.3 && std::fabs(y - 120) < 0.3);
}

ATF_TEST_CASE_WITHOUT_HEAD(outside_the_display_domain);
ATF_TEST_CASE_BODY(outside_the_display_domain)
{
    // UTM 33N cannot centre on New York: the geometry says so.
    const reproj::CrsRegistry registry = Registry();
    const app::MapGeometry geometry(registry.Find(25833), Camera(-74.0, 40.7, 10.0), 800, 600);
    ATF_REQUIRE(!geometry.Valid());
    double x, y;
    ATF_REQUIRE(!geometry.ToScreen(-74.0, 40.7, x, y));
}

ATF_TEST_CASE_WITHOUT_HEAD(source_grid_rules);
ATF_TEST_CASE_BODY(source_grid_rules)
{
    const reproj::CrsRegistry registry = Registry();
    app::MapViewModel vm;
    vm.width = 800;
    vm.height = 600;
    app::MapLayerInfo layer;
    layer.grids = {Grid(registry.Find(3857), -20037508.342789244, 20037508.342789244, 156543.03392804097, 19),
                   Grid(registry.Find(25833), -2500000.0, 9045984.0, 21664.0, 19)};
    vm.layers = {layer};
    vm.current = 0;
    vm.display = registry.Find(3857);
    // Web Mercator over the UTM zone: the local grid, warped.
    vm.overview = Camera(14.57, 68.23, 12.0);
    ATF_REQUIRE_EQ(app::SourceGrid(vm, app::MapGeometry(vm)), 1);
    // Within 3 degrees of its area (Bergen, 5.3 E; the zone starts at 12 E):
    // still not; Tromsø (18.96 E): yes.
    vm.overview = Camera(5.3, 60.4, 12.0);
    ATF_REQUIRE_EQ(app::SourceGrid(vm, app::MapGeometry(vm)), 0);
    vm.overview = Camera(18.96, 69.65, 12.0);
    ATF_REQUIRE_EQ(app::SourceGrid(vm, app::MapGeometry(vm)), 1);
    // Far away: the Web Mercator grid.
    vm.overview = Camera(-74.0, 40.7, 12.0);
    ATF_REQUIRE_EQ(app::SourceGrid(vm, app::MapGeometry(vm)), 0);
    // Shown in a system the layer has: its grid; in one it lacks
    // (Lambert-93 over Svolvær): the local grid again.
    vm.overview = Camera(14.57, 68.23, 12.0);
    vm.display = registry.Find(25833);
    ATF_REQUIRE_EQ(app::SourceGrid(vm, app::MapGeometry(vm)), 1);
    vm.display = registry.Find(2154);
    ATF_REQUIRE_EQ(app::SourceGrid(vm, app::MapGeometry(vm)), 1);
}

ATF_TEST_CASE_WITHOUT_HEAD(visible_tiles_limits_and_wrap);
ATF_TEST_CASE_BODY(visible_tiles_limits_and_wrap)
{
    const reproj::CrsRegistry registry = Registry();
    app::MapViewModel vm;
    vm.width = 800;
    vm.height = 600;
    app::MapLayerInfo layer;
    layer.grids = {Grid(registry.Find(3857), -20037508.342789244, 20037508.342789244, 156543.03392804097, 19)};
    vm.layers = {layer};
    vm.current = 0;
    vm.display = registry.Find(3857);
    // Across the antimeridian, Web Mercator wraps: columns stay in range.
    vm.overview = Camera(179.99, 0.0, 5.0);
    std::vector<app::VisibleTile> tiles = app::VisibleTiles(vm, app::MapGeometry(vm), 0);
    ATF_REQUIRE(!tiles.empty());
    bool west = false, east = false;
    for (const app::VisibleTile &tile : tiles) {
        ATF_REQUIRE(tile.key.x >= 0 && tile.key.x < 32);
        west = west || tile.key.x == 0;
        east = east || tile.key.x == 31;
    }
    ATF_REQUIRE(west && east);
    // The layer's limits keep requests to the tiles it holds.
    vm.layers[0].grids[0].tiles.levels[5].first_column = 17;
    vm.overview = Camera(0.0, 0.0, 5.0);
    for (const app::VisibleTile &tile : app::VisibleTiles(vm, app::MapGeometry(vm), 0))
        ATF_REQUIRE(tile.key.x >= 17);
    // Never more than kMaxVisibleTiles: a coarser level instead (a huge
    // viewport at a deep zoom).
    vm.width = 20000;
    vm.height = 20000;
    vm.overview = Camera(6.6, 45.9, 15.0);
    tiles = app::VisibleTiles(vm, app::MapGeometry(vm), 0);
    ATF_REQUIRE(!tiles.empty() && tiles.size() <= app::kMaxVisibleTiles);
    ATF_REQUIRE(tiles[0].key.z < 15);
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, screen_round_trip);
    ATF_ADD_TEST_CASE(tcs, zoom_keeps_the_point_under_the_mouse);
    ATF_ADD_TEST_CASE(tcs, outside_the_display_domain);
    ATF_ADD_TEST_CASE(tcs, source_grid_rules);
    ATF_ADD_TEST_CASE(tcs, visible_tiles_limits_and_wrap);
}
