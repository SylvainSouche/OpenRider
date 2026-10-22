// The plugin through its ABI.

#include <atf-c++.hpp>

#include <openrider/plugin.h>
#include <openrider/tiles.h>

#include "fixture.h"

#include <cstring>
#include <string>

extern "C" const or_plugin_info *openrider_plugin(void);

namespace {

const or_tiles_vtable *Vt()
{
    return static_cast<const or_tiles_vtable *>(openrider_plugin()->vtable);
}

const char *const kLayer = "{\"id\":\"scan25\",\"name\":\"Scan\",\"protocol\":\"wmts\","
                           "\"capabilities\":\"https://example.org/wmts?SERVICE=WMTS&REQUEST=GetCapabilities\","
                           "\"layers\":[\"scan\",\"scan.l93\"],\"headers\":{\"apikey\":\"key\"},"
                           "\"extent\":[-5,41,10,52],\"attribution\":\"(c) example\"}";

} // namespace

ATF_TEST_CASE_WITHOUT_HEAD(descriptor);
ATF_TEST_CASE_BODY(descriptor)
{
    ATF_REQUIRE_EQ(openrider_plugin()->kind, static_cast<uint32_t>(OR_PLUGIN_TILES));
    ATF_REQUIRE_EQ(openrider_plugin()->api_minor, 1u);
    ATF_REQUIRE_EQ(std::string(Vt()->protocol), "wmts");
    ATF_REQUIRE(Vt()->struct_size == sizeof(or_tiles_vtable));
}

ATF_TEST_CASE_WITHOUT_HEAD(grids_after_capabilities);
ATF_TEST_CASE_BODY(grids_after_capabilities)
{
    char err[256] = "";
    void *h = Vt()->open(kLayer, err, sizeof err);
    ATF_REQUIRE(h != nullptr);
    ATF_REQUIRE_EQ(std::string(Vt()->layer(h)->headers), "apikey: key\n");
    ATF_REQUIRE_EQ(std::string(Vt()->capabilities_url(h)), "https://example.org/wmts?SERVICE=WMTS&REQUEST=GetCapabilities");
    ATF_REQUIRE_EQ(Vt()->grid_count(h), 0u);  // until the capabilities arrive
    ATF_REQUIRE_EQ(Vt()->set_capabilities(h, kCapabilities, std::strlen(kCapabilities), err, sizeof err), 0);
    ATF_REQUIRE_EQ(Vt()->grid_count(h), 2u);
    ATF_REQUIRE_EQ(std::string(Vt()->grid(h, 1)->crs), "EPSG:2154");
    char url[512];
    ATF_REQUIRE_EQ(Vt()->grid_tile_url(h, 0, 1, 0, 3, 4, url, sizeof url), 0);
    ATF_REQUIRE_EQ(std::string(url), "https://example.org/wmts?SERVICE=WMTS&REQUEST=GetTile&VERSION=1.0.0&LAYER=scan.l93"
                                     "&STYLE=normal&TILEMATRIXSET=LAMB93&TILEMATRIX=1&TILEROW=4&TILECOL=3&FORMAT=image/jpeg");
    ATF_REQUIRE(Vt()->grid_tile_url(h, 0, 2, 0, 0, 0, url, sizeof url) != 0);  // no such grid
    ATF_REQUIRE(Vt()->tile_url(h, 0, 1, 0, 0, url, sizeof url) != 0);         // XYZ not served
    Vt()->close(h);
}

ATF_TEST_CASE_WITHOUT_HEAD(rest_urls);
ATF_TEST_CASE_BODY(rest_urls)
{
    char err[256] = "";
    void *h = Vt()->open("{\"id\":\"t\",\"protocol\":\"wmts\",\"capabilities\":\"https://example.org/caps.xml\",\"layers\":[\"topo\"]}",
                         err, sizeof err);
    ATF_REQUIRE(h != nullptr);
    ATF_REQUIRE_EQ(Vt()->set_capabilities(h, kCapabilities, std::strlen(kCapabilities), err, sizeof err), 0);
    char url[512];
    ATF_REQUIRE_EQ(Vt()->grid_tile_url(h, 0, 0, 1, 1, 0, url, sizeof url), 0);
    ATF_REQUIRE_EQ(std::string(url), "https://example.org/v1/topo/default/utm33n/01/0/1.png");
    Vt()->close(h);
}

ATF_TEST_CASE_WITHOUT_HEAD(refusals);
ATF_TEST_CASE_BODY(refusals)
{
    char err[256] = "";
    ATF_REQUIRE(Vt()->open("{\"protocol\":\"xyz\"}", err, sizeof err) == nullptr);
    ATF_REQUIRE(Vt()->open("{\"protocol\":\"wmts\",\"id\":\"x\"}", err, sizeof err) == nullptr);
    void *h = Vt()->open(kLayer, err, sizeof err);
    ATF_REQUIRE(Vt()->set_capabilities(h, "<not xml", 8, err, sizeof err) != 0);
    ATF_REQUIRE(std::string(err).find("capabilities") == 0);
    Vt()->close(h);
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, descriptor);
    ATF_ADD_TEST_CASE(tcs, grids_after_capabilities);
    ATF_ADD_TEST_CASE(tcs, rest_urls);
    ATF_ADD_TEST_CASE(tcs, refusals);
}
