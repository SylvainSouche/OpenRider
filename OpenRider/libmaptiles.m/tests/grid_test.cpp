// Tile grids, capabilities and grid-addressed fetching, offline: file://
// URLs stand for the service.

#include <atf-c++.hpp>

#include "maptiles/capabilities.h"
#include "maptiles/tile_fetcher.h"
#include "maptiles/tile_grid.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <utime.h>

namespace {

// 2x1 RGBA PNG: a red pixel, then a blue one.
const char kPng[] =
    "\x89\x50\x4e\x47\x0d\x0a\x1a\x0a\x00\x00\x00\x0d\x49\x48\x44\x52\x00\x00\x00\x02\x00\x00"
    "\x00\x01\x08\x06\x00\x00\x00\xf4\x22\x7f\x8a\x00\x00\x00\x0e\x49\x44\x41\x54\x78\x9c\x63"
    "\xf8\xcf\xc0\x00\x42\xff\x01\x0f\xf9\x03\xfd\x85\x11\x99\x76\x00\x00\x00\x00\x49\x45\x4e"
    "\x44\xae\x42\x60\x82";

std::string TemporaryDirectory()
{
    char path[] = "/tmp/openrider-grid-XXXXXX";
    return mkdtemp(path);
}

void Write(const std::string &path, const std::string &bytes)
{
    FILE *file = std::fopen(path.c_str(), "wb");
    std::fwrite(bytes.data(), 1, bytes.size(), file);
    std::fclose(file);
}

std::string Read(const std::string &path)
{
    std::string text;
    if (FILE *file = std::fopen(path.c_str(), "rb")) {
        char chunk[4096];
        size_t got;
        while ((got = std::fread(chunk, 1, sizeof chunk, file)) > 0)
            text.append(chunk, got);
        std::fclose(file);
    }
    return text;
}

// A plugin with one grid ("utm", EPSG:25833, two levels) once given a
// capabilities document "grids", whose URLs point into `g_root`.
std::string g_root, g_capabilities_url, g_received;
or_tile_layer g_info{"layer", "Layer", "EPSG:3857", 0, 3, 256, {-180, -85, 180, 85}, "", 1, ""};
or_tile_matrix g_levels[2] = {{"a", 1000.0, 0.0, 1000000.0, 256, 256, 4, 4, 0, 3, 0, 3},
                              {"b", 500.0, 0.0, 1000000.0, 256, 256, 8, 8, 0, 7, 0, 7}};
or_tile_grid g_grid{"utm", "EPSG:25833", 2, g_levels};
bool g_ready = false;

void *Open(const char *, char *, size_t) { return &g_info; }
void Close(void *) {}
const or_tile_layer *Layer(void *) { return &g_info; }
int XyzUrl(void *, size_t, int, int, int, char *, size_t) { return 1; }
const char *CapabilitiesUrl(void *) { return g_capabilities_url.c_str(); }
int SetCapabilities(void *, const char *document, size_t len, char *err, size_t errlen)
{
    g_received.assign(document, len);
    g_ready = g_received == "grids";
    if (!g_ready)
        std::snprintf(err, errlen, "not grids");
    return g_ready ? 0 : 1;
}
size_t GridCount(void *) { return g_ready ? 1 : 0; }
const or_tile_grid *Grid(void *, size_t) { return &g_grid; }
int GridUrl(void *, size_t, size_t, size_t level, int column, int row, char *url, size_t len)
{
    std::snprintf(url, len, "file://%s/%zu-%d-%d.png", g_root.c_str(), level, column, row);
    return 0;
}
const or_tiles_vtable kGrids = {sizeof(or_tiles_vtable), "grids", Open, Close, Layer, XyzUrl,
                                CapabilitiesUrl, SetCapabilities, GridCount, Grid, GridUrl};
// The same grids, served by a host refusing connections (port 9, closed).
int RefusedUrl(void *, size_t, size_t, size_t level, int column, int row, char *url, size_t len)
{
    std::snprintf(url, len, "http://127.0.0.1:9/%zu/%d/%d.png", level, column, row);
    return 0;
}
const or_tiles_vtable kRefused = {sizeof(or_tiles_vtable), "refused", Open, Close, Layer, XyzUrl,
                                  CapabilitiesUrl, SetCapabilities, GridCount, Grid, RefusedUrl};
const or_tiles_vtable kXyz = {offsetof(or_tiles_vtable, capabilities_url), "xyz", Open, Close, Layer, XyzUrl,
                              nullptr, nullptr, nullptr, nullptr, nullptr};

} // namespace

ATF_TEST_CASE_WITHOUT_HEAD(web_mercator_grid_of_a_plain_layer);
ATF_TEST_CASE_BODY(web_mercator_grid_of_a_plain_layer)
{
    // An ABI 2.0 table: no grid functions, one derived grid.
    ATF_REQUIRE(!maptiles::HasGridFunctions(&kXyz));
    const std::vector<maptiles::TileGrid> grids = maptiles::GridsOf(&kXyz, &g_info);
    ATF_REQUIRE_EQ(grids.size(), 1u);
    const maptiles::TileGrid &grid = grids[0];
    ATF_REQUIRE(grid.xyz);
    ATF_REQUIRE_EQ(grid.crs, "EPSG:3857");
    ATF_REQUIRE_EQ(grid.levels.size(), 4u);  // levels 0..max_zoom
    ATF_REQUIRE_EQ(grid.levels[3].matrix_width, 8);
    ATF_REQUIRE(std::fabs(grid.levels[0].resolution - 156543.03392804097) < 1e-6);
    ATF_REQUIRE_EQ(maptiles::CacheName("layer", grid), "layer");
}

ATF_TEST_CASE_WITHOUT_HEAD(capabilities_then_grids);
ATF_TEST_CASE_BODY(capabilities_then_grids)
{
    g_root = TemporaryDirectory();
    const std::string cache = TemporaryDirectory();
    g_capabilities_url = "file://" + g_root + "/caps.xml";
    Write(g_root + "/caps.xml", "grids");
    g_ready = false;
    ATF_REQUIRE(maptiles::HasGridFunctions(&kGrids));
    ATF_REQUIRE_EQ(maptiles::ProvideCapabilities(&kGrids, &g_info, cache, "test"), "");
    ATF_REQUIRE_EQ(Read(cache + "/layer/capabilities.xml"), "grids");  // cached
    const std::vector<maptiles::TileGrid> grids = maptiles::GridsOf(&kGrids, &g_info);
    ATF_REQUIRE_EQ(grids.size(), 1u);
    ATF_REQUIRE(!grids[0].xyz);
    ATF_REQUIRE_EQ(grids[0].crs, "EPSG:25833");
    ATF_REQUIRE_EQ(grids[0].levels[1].id, "b");
    ATF_REQUIRE_EQ(maptiles::CacheName("layer", grids[0]), "layer@utm");

    // Recent: the cached copy, even when the service changed.
    Write(g_root + "/caps.xml", "changed");
    ATF_REQUIRE_EQ(maptiles::ProvideCapabilities(&kGrids, &g_info, cache, "test"), "");
    ATF_REQUIRE_EQ(g_received, "grids");
    // A week old: downloaded again.
    const time_t old = std::time(nullptr) - 8 * 24 * 3600;
    const utimbuf times{old, old};
    utime((cache + "/layer/capabilities.xml").c_str(), &times);
    ATF_REQUIRE_EQ(maptiles::ProvideCapabilities(&kGrids, &g_info, cache, "test"), "not grids");
    ATF_REQUIRE_EQ(g_received, "changed");
    // Unreachable service, stale copy: the stale copy is used.
    Write(cache + "/layer/capabilities.xml", "grids");
    utime((cache + "/layer/capabilities.xml").c_str(), &times);
    std::remove((g_root + "/caps.xml").c_str());
    ATF_REQUIRE_EQ(maptiles::ProvideCapabilities(&kGrids, &g_info, cache, "test"), "");
    ATF_REQUIRE_EQ(g_received, "grids");
    // Unreachable, nothing cached: refused.
    const std::string empty = TemporaryDirectory();
    ATF_REQUIRE(maptiles::ProvideCapabilities(&kGrids, &g_info, empty, "test").find("cannot download") == 0);
}

ATF_TEST_CASE_WITHOUT_HEAD(fetches_by_grid);
ATF_TEST_CASE_BODY(fetches_by_grid)
{
    g_root = TemporaryDirectory();
    const std::string cache_root = TemporaryDirectory();
    g_ready = true;
    Write(g_root + "/1-2-3.png", std::string(kPng, sizeof kPng - 1));
    maptiles::TileCache cache(cache_root, 1 << 20);
    maptiles::TileFetcher fetcher(&kGrids, &g_info, maptiles::GridsOf(&kGrids, &g_info), &cache, "test", 1);
    maptiles::TileKey present{1, 2, 3, 0}, absent{1, 5, 5, 0};
    fetcher.Request(present);
    fetcher.Request(absent);
    std::vector<maptiles::ReadyTile> ready;
    for (int i = 0; i < 100 && ready.size() < 2; ++i) {
        for (maptiles::ReadyTile &tile : fetcher.TakeReady())
            ready.push_back(std::move(tile));
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    ATF_REQUIRE_EQ(ready.size(), 2u);
    for (const maptiles::ReadyTile &tile : ready)
        ATF_REQUIRE_EQ(tile.image.rgba.empty(), tile.key == absent);
    ATF_REQUIRE_EQ(fetcher.Errors(), 0u);  // a missing tile is not a fault
    // Cached under the grid's name.
    std::string bytes;
    ATF_REQUIRE(cache.Get("layer@utm", 1, 2, 3, bytes));
}

ATF_TEST_CASE_WITHOUT_HEAD(unreachable_service);
ATF_TEST_CASE_BODY(unreachable_service)
{
    // Every download fails: each tile counts as an error (not as missing),
    // and after three the service is unavailable. The delay between
    // attempts grows (100 ms, then x2 + 500 ms), so this takes about 3 s.
    g_ready = true;
    maptiles::TileCache cache(TemporaryDirectory(), 1 << 20);
    maptiles::TileFetcher fetcher(&kRefused, &g_info, maptiles::GridsOf(&kRefused, &g_info), &cache, "test", 1);
    for (int column = 0; column < 3; ++column)
        fetcher.Request(maptiles::TileKey{0, column, 0, 0});
    size_t ready = 0;
    for (int i = 0; i < 300 && ready < 3; ++i) {
        for (const maptiles::ReadyTile &tile : fetcher.TakeReady()) {
            ATF_REQUIRE(tile.image.rgba.empty());
            ++ready;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    ATF_REQUIRE_EQ(ready, 3u);
    ATF_REQUIRE_EQ(fetcher.Errors(), 3u);
    ATF_REQUIRE(fetcher.Unavailable());
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, unreachable_service);
    ATF_ADD_TEST_CASE(tcs, web_mercator_grid_of_a_plain_layer);
    ATF_ADD_TEST_CASE(tcs, capabilities_then_grids);
    ATF_ADD_TEST_CASE(tcs, fetches_by_grid);
}
