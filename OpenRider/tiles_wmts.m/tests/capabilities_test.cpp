// Tile grids read from WMTS capabilities.

#include <atf-c++.hpp>

#include "../src/wmts_capabilities.h"
#include "fixture.h"

#include <cmath>

namespace {

std::vector<WmtsGrid> Grids(const std::vector<std::string> &layers, std::string &kvp_url, std::string &error)
{
    std::string parse_error;
    const auto root = ParseXml(kCapabilities, parse_error);
    ATF_REQUIRE(root != nullptr);
    std::vector<WmtsGrid> grids;
    error = ReadGrids(*root, layers, grids, kvp_url);
    return grids;
}

} // namespace

ATF_TEST_CASE_WITHOUT_HEAD(crs_names);
ATF_TEST_CASE_BODY(crs_names)
{
    ATF_REQUIRE_EQ(NormalisedCrs("urn:ogc:def:crs:EPSG::25833"), "EPSG:25833");
    ATF_REQUIRE_EQ(NormalisedCrs("urn:ogc:def:crs:EPSG:6.18:3:3857"), "EPSG:3857");
    ATF_REQUIRE_EQ(NormalisedCrs("http://www.opengis.net/def/crs/EPSG/0/2154"), "EPSG:2154");
    ATF_REQUIRE_EQ(NormalisedCrs("EPSG:4326"), "EPSG:4326");
    ATF_REQUIRE_EQ(NormalisedCrs("IGNF:LAMB93"), "EPSG:2154");
    ATF_REQUIRE_EQ(NormalisedCrs("urn:ogc:def:crs:OGC:1.3:CRS84"), "CRS84");
    ATF_REQUIRE_EQ(NormalisedCrs("IGNF:GEOPORTALFXX"), "IGNF:GEOPORTALFXX");
}

ATF_TEST_CASE_WITHOUT_HEAD(rest_layer_in_two_systems);
ATF_TEST_CASE_BODY(rest_layer_in_two_systems)
{
    std::string kvp, error;
    const std::vector<WmtsGrid> grids = Grids({"topo"}, kvp, error);
    ATF_REQUIRE_EQ(error, "");
    ATF_REQUIRE_EQ(kvp, "https://example.org/wmts?");
    ATF_REQUIRE_EQ(grids.size(), 2u);
    ATF_REQUIRE_EQ(grids[0].crs, "EPSG:25833");
    ATF_REQUIRE_EQ(grids[1].crs, "EPSG:3857");
    const WmtsGrid &utm = grids[0];
    // Sorted coarsest first; scale denominator x 0.28 mm is the pixel.
    ATF_REQUIRE_EQ(utm.level_ids[0], "00");
    ATF_REQUIRE(std::fabs(utm.levels[0].resolution - 21664.0) < 1e-6);
    ATF_REQUIRE_EQ(std::string(utm.levels[1].id), "01");
    ATF_REQUIRE_EQ(utm.levels[0].origin_x, -2500000.0);
    ATF_REQUIRE_EQ(utm.levels[0].origin_y, 9045984.0);
    ATF_REQUIRE_EQ(utm.levels[1].last_column, 1);
    ATF_REQUIRE_EQ(utm.style, "default");
    ATF_REQUIRE(utm.url_template.find("{TileMatrixSet}") != std::string::npos);
    ATF_REQUIRE(std::fabs(grids[1].levels[0].resolution - 156543.03392804097) < 1e-6);
}

ATF_TEST_CASE_WITHOUT_HEAD(two_layers_one_grid_each_with_limits);
ATF_TEST_CASE_BODY(two_layers_one_grid_each_with_limits)
{
    std::string kvp, error;
    const std::vector<WmtsGrid> grids = Grids({"scan", "scan.l93"}, kvp, error);
    ATF_REQUIRE_EQ(error, "");
    ATF_REQUIRE_EQ(grids.size(), 2u);
    ATF_REQUIRE_EQ(grids[0].layer, "scan");
    ATF_REQUIRE_EQ(grids[1].layer, "scan.l93");
    ATF_REQUIRE_EQ(grids[1].crs, "EPSG:2154");
    // Only the level the layer's limits list, within them.
    ATF_REQUIRE_EQ(grids[1].levels.size(), 1u);
    ATF_REQUIRE_EQ(grids[1].level_ids[0], "1");
    ATF_REQUIRE_EQ(grids[1].levels[0].first_row, 2);
    ATF_REQUIRE_EQ(grids[1].levels[0].last_column, 4);
    ATF_REQUIRE(grids[1].url_template.empty());  // KVP
}

ATF_TEST_CASE_WITHOUT_HEAD(northing_first_corner);
ATF_TEST_CASE_BODY(northing_first_corner)
{
    // EPSG:3035 lists northing first: the corner is swapped into x, y.
    std::string kvp, error;
    const std::vector<WmtsGrid> grids = Grids({"laea"}, kvp, error);
    ATF_REQUIRE_EQ(error, "");
    ATF_REQUIRE_EQ(grids[0].levels[0].origin_x, 1500000.0);
    ATF_REQUIRE_EQ(grids[0].levels[0].origin_y, 5500000.0);
}

ATF_TEST_CASE_WITHOUT_HEAD(missing_layer);
ATF_TEST_CASE_BODY(missing_layer)
{
    std::string kvp, error;
    Grids({"nowhere"}, kvp, error);
    ATF_REQUIRE_EQ(error, "no layer nowhere in the capabilities");
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, crs_names);
    ATF_ADD_TEST_CASE(tcs, rest_layer_in_two_systems);
    ATF_ADD_TEST_CASE(tcs, two_layers_one_grid_each_with_limits);
    ATF_ADD_TEST_CASE(tcs, northing_first_corner);
    ATF_ADD_TEST_CASE(tcs, missing_layer);
}
