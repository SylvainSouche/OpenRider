// The interpolating projector and the local scale factor.

#include <atf-c++.hpp>

#include "reproj/grid_projector.h"
#include "shipped.h"

#include <cmath>

ATF_TEST_CASE_WITHOUT_HEAD(interpolation_within_tolerance);
ATF_TEST_CASE_BODY(interpolation_within_tolerance)
{
    // The Lofoten, about 60 x 30 km, in UTM 33N to 1 m: every point of a
    // 100 x 100 sweep within the tolerance of the exact transformation.
    const reproj::Transformer exact(ShippedCrs(4326), ShippedCrs(25833));
    const reproj::Box box{13.5, 68.5, 15.5, 68.0};
    const reproj::GridProjector projector(exact, box, 1.0);
    for (int i = 0; i <= 100; ++i)
        for (int j = 0; j <= 100; ++j) {
            const double lon = 13.5 + 2.0 * i / 100, lat = 68.0 + 0.5 * j / 100;
            double x, y, ex, ey;
            ATF_REQUIRE(projector.Project(lon, lat, x, y));
            ATF_REQUIRE(exact.Transform(lon, lat, ex, ey));
            ATF_REQUIRE(std::hypot(x - ex, y - ey) <= 1.0);
        }
}

ATF_TEST_CASE_WITHOUT_HEAD(outside_the_box_is_exact);
ATF_TEST_CASE_BODY(outside_the_box_is_exact)
{
    const reproj::Transformer exact(ShippedCrs(4326), ShippedCrs(2154));
    const reproj::GridProjector projector(exact, {5.0, 47.0, 7.0, 45.0}, 1.0);
    double x, y, ex, ey;
    ATF_REQUIRE(projector.Project(2.3488, 48.8534, x, y));  // Paris, outside
    ATF_REQUIRE(exact.Transform(2.3488, 48.8534, ex, ey));
    ATF_REQUIRE_EQ(x, ex);
    ATF_REQUIRE_EQ(y, ey);
    // Undefined points stay undefined.
    ATF_REQUIRE(!projector.Project(0.0, -90.0, x, y));
}

ATF_TEST_CASE_WITHOUT_HEAD(unreachable_tolerance_falls_back_to_exact);
ATF_TEST_CASE_BODY(unreachable_tolerance_falls_back_to_exact)
{
    // The whole of Europe in Web Mercator to a millimetre with 4 cells:
    // the grid is not used, results stay exact.
    const reproj::Transformer exact(ShippedCrs(4326), ShippedCrs(3857));
    const reproj::GridProjector projector(exact, {-10.0, 70.0, 30.0, 35.0}, 0.001, 4);
    double x, y, ex, ey;
    ATF_REQUIRE(projector.Project(10.1, 52.3, x, y));
    ATF_REQUIRE(exact.Transform(10.1, 52.3, ex, ey));
    ATF_REQUIRE_EQ(x, ex);
    ATF_REQUIRE_EQ(y, ey);
}

ATF_TEST_CASE_WITHOUT_HEAD(scale_factors);
ATF_TEST_CASE_BODY(scale_factors)
{
    // UTM: 0.9996 on the central meridian; Mercator: about 1/cos(lat)
    // (the sphere's against the ellipsoid's meridian radius at 68 N).
    ATF_REQUIRE(std::fabs(reproj::ScaleFactor(ShippedCrs(25833), 15.0, 68.0) - 0.9996) < 1e-6);
    const double mercator = reproj::ScaleFactor(ShippedCrs(3857), 15.0, 68.0);
    ATF_REQUIRE(std::fabs(mercator * std::cos(68.0 * 3.14159265358979 / 180.0) - 1.0) < 0.01);
    // Geographic: degrees per metre, about 1 / 111.6 km at 68 N.
    ATF_REQUIRE(std::fabs(reproj::ScaleFactor(ShippedCrs(4326), 15.0, 68.0) * 111600.0 - 1.0) < 0.01);
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, interpolation_within_tolerance);
    ATF_ADD_TEST_CASE(tcs, outside_the_box_is_exact);
    ATF_ADD_TEST_CASE(tcs, unreachable_tolerance_falls_back_to_exact);
    ATF_ADD_TEST_CASE(tcs, scale_factors);
}
