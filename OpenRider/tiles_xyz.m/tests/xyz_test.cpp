#include <atf-c++.hpp>

#include <openrider/plugin.h>
#include <openrider/tiles.h>

#include <string>

extern "C" const or_plugin_info *openrider_plugin(void);

namespace {

const or_tiles_vtable *vt()
{
    return static_cast<const or_tiles_vtable *>(openrider_plugin()->vtable);
}

const char *kOpenTopo =
    "{\"id\":\"opentopomap\",\"name\":\"OpenTopoMap\",\"protocol\":\"xyz\","
    " \"url\":\"https://{s}.tile.opentopomap.org/{z}/{x}/{y}.png\","
    " \"servers\":[\"a\",\"b\",\"c\"],\"max_zoom\":17,"
    " \"attribution\":\"Map data: (c) OpenStreetMap contributors\","
    " \"headers\":{\"apikey\":\"secret\"}}";

} // namespace

ATF_TEST_CASE_WITHOUT_HEAD(descriptor);
ATF_TEST_CASE_BODY(descriptor)
{
    ATF_REQUIRE_EQ(openrider_plugin()->kind, static_cast<uint32_t>(OR_PLUGIN_TILES));
    ATF_REQUIRE_EQ(std::string(vt()->protocol), "xyz");
}

ATF_TEST_CASE_WITHOUT_HEAD(urls_rotate_over_servers);
ATF_TEST_CASE_BODY(urls_rotate_over_servers)
{
    char err[128] = "";
    void *h = vt()->open(kOpenTopo, err, sizeof err);
    ATF_REQUIRE(h != nullptr);
    char url[256];
    ATF_REQUIRE_EQ(vt()->tile_url(h, 1, 12, 2121, 1458, url, sizeof url), 0);
    ATF_REQUIRE_EQ(std::string(url), "https://b.tile.opentopomap.org/12/2121/1458.png");
    ATF_REQUIRE(vt()->tile_url(h, 3, 12, 0, 0, url, sizeof url) != 0);  // no 4th server
    vt()->close(h);
}

ATF_TEST_CASE_WITHOUT_HEAD(layer_description);
ATF_TEST_CASE_BODY(layer_description)
{
    char err[128] = "";
    void *h = vt()->open(kOpenTopo, err, sizeof err);
    const or_tile_layer *l = vt()->layer(h);
    ATF_REQUIRE_EQ(std::string(l->id), "opentopomap");
    ATF_REQUIRE_EQ(std::string(l->crs), "EPSG:3857");
    ATF_REQUIRE_EQ(l->max_zoom, 17);
    ATF_REQUIRE_EQ(l->server_count, 3u);
    ATF_REQUIRE_EQ(std::string(l->headers), "apikey: secret\n");
    ATF_REQUIRE(l->extent[0] == -180.0 && l->extent[2] == 180.0);  // world by default
    vt()->close(h);
}

ATF_TEST_CASE_WITHOUT_HEAD(rejects_bad_definitions);
ATF_TEST_CASE_BODY(rejects_bad_definitions)
{
    char err[128] = "";
    ATF_REQUIRE(vt()->open("{\"id\":\"x\",\"url\":\"https://t/{z}/{x}.png\"}", err, sizeof err) == nullptr);
    ATF_REQUIRE(vt()->open("{\"id\":\"x\",\"url\":\"https://t/{z}/{x}/{y}\",\"crs\":\"EPSG:2154\"}",
                           err, sizeof err) == nullptr);
    ATF_REQUIRE(std::string(err).find("EPSG:2154") != std::string::npos);
    ATF_REQUIRE(vt()->open("not json", err, sizeof err) == nullptr);
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, descriptor);
    ATF_ADD_TEST_CASE(tcs, urls_rotate_over_servers);
    ATF_ADD_TEST_CASE(tcs, layer_description);
    ATF_ADD_TEST_CASE(tcs, rejects_bad_definitions);
}
