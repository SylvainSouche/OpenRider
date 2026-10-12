#include <atf-c++.hpp>

#include "maptiles/mercator.h"

#include <cmath>

ATF_TEST_CASE_WITHOUT_HEAD(origin_and_corners);
ATF_TEST_CASE_BODY(origin_and_corners)
{
    double x, y;
    maptiles::lonlat_to_world(0.0, 0.0, 0, 256, x, y);
    ATF_REQUIRE(std::fabs(x - 128.0) < 1e-9 && std::fabs(y - 128.0) < 1e-9);
    maptiles::lonlat_to_world(-180.0, maptiles::kMaxLatitude, 1, 256, x, y);
    ATF_REQUIRE(std::fabs(x) < 1e-9 && std::fabs(y) < 1e-6);
}

ATF_TEST_CASE_WITHOUT_HEAD(known_tile);
ATF_TEST_CASE_BODY(known_tile)
{
    // Sallanches (45.936N, 6.632E) lies in OSM tile z=12 x=2123 y=1458.
    double x, y;
    maptiles::lonlat_to_world(6.632, 45.936, 12, 256, x, y);
    ATF_REQUIRE_EQ(static_cast<int>(x / 256), 2123);
    ATF_REQUIRE_EQ(static_cast<int>(y / 256), 1458);
}

ATF_TEST_CASE_WITHOUT_HEAD(round_trip);
ATF_TEST_CASE_BODY(round_trip)
{
    double x, y, lon, lat;
    maptiles::lonlat_to_world(6.6183, 45.9415, 15.5, 256, x, y);
    maptiles::world_to_lonlat(x, y, 15.5, 256, lon, lat);
    ATF_REQUIRE(std::fabs(lon - 6.6183) < 1e-9 && std::fabs(lat - 45.9415) < 1e-9);
}

ATF_TEST_CASE_WITHOUT_HEAD(fit_zoom_frames_box);
ATF_TEST_CASE_BODY(fit_zoom_frames_box)
{
    // A ~10 km box needs zoom 12 in a 600x400 view; the max zoom caps it.
    ATF_REQUIRE_EQ(maptiles::fit_zoom(6.55, 45.90, 6.70, 45.97, 600, 400, 256, 1, 17), 12);
    ATF_REQUIRE_EQ(maptiles::fit_zoom(6.60, 45.94, 6.6001, 45.9401, 600, 400, 256, 1, 17), 17);
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, origin_and_corners);
    ATF_ADD_TEST_CASE(tcs, known_tile);
    ATF_ADD_TEST_CASE(tcs, round_trip);
    ATF_ADD_TEST_CASE(tcs, fit_zoom_frames_box);
}
