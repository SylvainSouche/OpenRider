// Transformations between systems, one point and in batches.

#include <atf-c++.hpp>

#include "reproj/transformer.h"
#include "shipped.h"

#include <cmath>
#include <vector>

ATF_TEST_CASE_WITHOUT_HEAD(web_mercator_to_utm);
ATF_TEST_CASE_BODY(web_mercator_to_utm)
{
    // Svolvær, from its Web Mercator position to UTM 33N (PROJ's values).
    const reproj::Transformer transformer(ShippedCrs(3857), ShippedCrs(25833));
    double x, y;
    ATF_REQUIRE(transformer.Transform(1621746.8697, 10517067.7830, x, y));
    ATF_REQUIRE(std::fabs(x - 482140.0044) < 0.002 && std::fabs(y - 7569082.9568) < 0.002);
    double back_x, back_y;
    ATF_REQUIRE(transformer.Inverse().Transform(x, y, back_x, back_y));
    ATF_REQUIRE(std::fabs(back_x - 1621746.8697) < 1e-6 && std::fabs(back_y - 10517067.7830) < 1e-6);
}

ATF_TEST_CASE_WITHOUT_HEAD(batch_matches_single_points);
ATF_TEST_CASE_BODY(batch_matches_single_points)
{
    // Enough vertices to be spread over several threads, a few failing.
    const reproj::Transformer transformer(ShippedCrs(4326), ShippedCrs(2154));
    const size_t count = 200000;
    std::vector<double> xs(count), ys(count);
    for (size_t i = 0; i < count; ++i) {
        xs[i] = -5.0 + 15.0 * i / count;
        ys[i] = 42.0 + 9.0 * i / count;
    }
    xs[7] = std::nan("");
    ys[11] = -90.0;  // the south pole: undefined in Lambert-93
    std::vector<double> expected_x(xs), expected_y(ys);
    ATF_REQUIRE_EQ(transformer.TransformPoints(xs.data(), ys.data(), count), 2u);
    ATF_REQUIRE(std::isnan(xs[7]) && std::isnan(ys[11]));
    for (size_t i = 0; i < count; i += 997) {
        if (i == 7 || i == 11)
            continue;
        double x, y;
        ATF_REQUIRE(transformer.Transform(expected_x[i], expected_y[i], x, y));
        ATF_REQUIRE_EQ(xs[i], x);
        ATF_REQUIRE_EQ(ys[i], y);
    }
}

ATF_TEST_CASE_WITHOUT_HEAD(identity_copies);
ATF_TEST_CASE_BODY(identity_copies)
{
    const reproj::Transformer transformer(ShippedCrs(2154), ShippedCrs(2154));
    double x, y;
    ATF_REQUIRE(transformer.Transform(1.0e9, -3.0, x, y));  // not even checked against the projection
    ATF_REQUIRE_EQ(x, 1.0e9);
    ATF_REQUIRE_EQ(y, -3.0);
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, web_mercator_to_utm);
    ATF_ADD_TEST_CASE(tcs, batch_matches_single_points);
    ATF_ADD_TEST_CASE(tcs, identity_copies);
}
