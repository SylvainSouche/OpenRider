// Coordinate systems declared as text.

#include <atf-c++.hpp>

#include "reproj/crs_registry.h"
#include "shipped.h"

#include <cmath>

ATF_TEST_CASE_WITHOUT_HEAD(builtins_without_a_file);
ATF_TEST_CASE_BODY(builtins_without_a_file)
{
    const reproj::CrsRegistry registry;
    ATF_REQUIRE(registry.Find(4326).geographic);
    ATF_REQUIRE_EQ(registry.Find("epsg:3857").name, "WGS 84 / Pseudo-Mercator");
    ATF_REQUIRE_EQ(registry.Codes().size(), 2u);
}

ATF_TEST_CASE_WITHOUT_HEAD(shipped_systems);
ATF_TEST_CASE_BODY(shipped_systems)
{
    // Every line of the shipped file parsed (Shipped() fails otherwise).
    ATF_REQUIRE_EQ(ShippedCrs(2154).name, "RGF93 v1 / Lambert-93");
    ATF_REQUIRE_EQ(ShippedCrs(25833).name, "ETRS89 / UTM zone 33N");
    ATF_REQUIRE(ShippedCrs(28992).datum.shifted);    // Amersfoort: Bessel, shifted
    ATF_REQUIRE(!ShippedCrs(3035).datum.shifted);    // ETRS89: taken as WGS 84
    ATF_REQUIRE(Shipped().Codes().size() > 150u);
    ATF_REQUIRE(!Shipped().Find(2153).Valid());
    ATF_REQUIRE(!Shipped().Find("EPSG:2154x").Valid());
}

ATF_TEST_CASE_WITHOUT_HEAD(a_line_adds_a_system);
ATF_TEST_CASE_BODY(a_line_adds_a_system)
{
    reproj::CrsRegistry registry;
    const std::vector<std::string> errors = registry.Parse(
        "# a comment\n"
        "\n"
        "EPSG:2056 | CH1903+ / LV95 | 5.95,45.81,10.5,47.81 | +proj=somerc +lat_0=46.9524055555556 "
        "+lon_0=7.43958333333333 +k_0=1 +x_0=2600000 +y_0=1200000 +ellps=bessel "
        "+towgs84=674.374,15.056,405.346,0,0,0,0 +units=m\n");
    ATF_REQUIRE(errors.empty());
    // Bern's old observatory, the origin of LV95.
    double x, y;
    const reproj::Crs lv95 = registry.Find(2056);
    ATF_REQUIRE(lv95.projection->Forward(7.43958333333333, 46.9524055555556, x, y));
    ATF_REQUIRE(std::fabs(x - 2600000.0) < 1e-6 && std::fabs(y - 1200000.0) < 1e-6);
}

ATF_TEST_CASE_WITHOUT_HEAD(bad_lines_are_refused);
ATF_TEST_CASE_BODY(bad_lines_are_refused)
{
    reproj::CrsRegistry registry;
    const std::vector<std::string> errors = registry.Parse(
        "EPSG:1 | Missing fields\n"
        "EPSG:2 | Bad area | 10,0,0,10 | +proj=utm +zone=31 +ellps=GRS80\n"
        "EPSG:3 | Unknown method | 0,0,1,1 | +proj=robin +ellps=GRS80\n"
        "EPSG:4 | Grid shift | 0,0,1,1 | +proj=tmerc +ellps=GRS80 +nadgrids=ntf_r93.gsb\n"
        "EPSG:5 | Feet | 0,0,1,1 | +proj=tmerc +ellps=GRS80 +units=us-ft\n"
        "EPSG:6 | Unknown parameter | 0,0,1,1 | +proj=tmerc +ellps=GRS80 +lat_2=4\n"
        "EPSG:7 | Unknown ellipsoid | 0,0,1,1 | +proj=tmerc +ellps=everest\n"
        "EPSG:8 | Paris meridian | 0,0,1,1 | +proj=lcc +lat_1=46.8 +pm=paris +ellps=clrk80ign\n"
        "XYZ:9 | Not EPSG | 0,0,1,1 | +proj=longlat +datum=WGS84\n");
    ATF_REQUIRE_EQ(errors.size(), 9u);
    ATF_REQUIRE(errors[2].find("line 3: EPSG:3: unsupported projection method: robin") == 0);
    ATF_REQUIRE_EQ(registry.Codes().size(), 2u);  // only the built-ins
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, builtins_without_a_file);
    ATF_ADD_TEST_CASE(tcs, shipped_systems);
    ATF_ADD_TEST_CASE(tcs, a_line_adds_a_system);
    ATF_ADD_TEST_CASE(tcs, bad_lines_are_refused);
}
