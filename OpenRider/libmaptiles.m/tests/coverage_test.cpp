#include <atf-c++.hpp>

#include "maptiles/coverage.h"

#include <cmath>

namespace {
const double kFrance[4] = {-5.7, 41.2, 10.9, 51.2};
const double kWorld[4] = {-180, -85.05, 180, 85.05};
} // namespace

ATF_TEST_CASE_WITHOUT_HEAD(view_fraction);
ATF_TEST_CASE_BODY(view_fraction)
{
    ATF_REQUIRE_EQ(maptiles::view_fraction_inside({6.5, 45.8, 6.8, 46.0}, kFrance), 1.0);
    // Half of this view lies east of the extent.
    ATF_REQUIRE(std::fabs(maptiles::view_fraction_inside({10.4, 45.0, 11.4, 46.0}, kFrance) - 0.5) < 1e-9);
    ATF_REQUIRE_EQ(maptiles::view_fraction_inside({20, 45, 21, 46}, kFrance), 0.0);
}

ATF_TEST_CASE_WITHOUT_HEAD(alps_outing_offers_france_layer);
ATF_TEST_CASE_BODY(alps_outing_offers_france_layer)
{
    const std::vector<double> lons = {6.60, 6.62, 6.65}, lats = {45.90, 45.93, 45.95};
    ATF_REQUIRE(maptiles::layer_offered(kFrance, {6.5, 45.8, 6.8, 46.0}, lons, lats));
    ATF_REQUIRE(maptiles::layer_offered(kWorld, {6.5, 45.8, 6.8, 46.0}, lons, lats));
}

ATF_TEST_CASE_WITHOUT_HEAD(outing_leaving_the_extent_is_not_covered);
ATF_TEST_CASE_BODY(outing_leaving_the_extent_is_not_covered)
{
    // A ride into Switzerland/Italy past 10.9E: 1 of 3 points outside.
    const std::vector<double> lons = {10.5, 10.8, 11.3}, lats = {46.0, 46.1, 46.2};
    ATF_REQUIRE(!maptiles::layer_offered(kFrance, {10.4, 45.9, 10.8, 46.3}, lons, lats));
    // Seen from far out, the view itself leaves the extent.
    ATF_REQUIRE(!maptiles::layer_offered(kFrance, {0, 40, 20, 52}, {}, {}));
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, view_fraction);
    ATF_ADD_TEST_CASE(tcs, alps_outing_offers_france_layer);
    ATF_ADD_TEST_CASE(tcs, outing_leaving_the_extent_is_not_covered);
}
