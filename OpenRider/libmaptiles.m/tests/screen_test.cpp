#include <atf-c++.hpp>

#include "maptiles/screen.h"

ATF_TEST_CASE_WITHOUT_HEAD(sub_pixel_zigzag_collapses);
ATF_TEST_CASE_BODY(sub_pixel_zigzag_collapses)
{
    // 100 points zigzagging +/-0.3 px while advancing 0.1 px each: ~10 px of
    // track; only points >= 1.5 px apart survive.
    std::vector<float> xs, ys;
    for (int i = 0; i < 100; ++i) {
        xs.push_back(i * 0.1f);
        ys.push_back(i % 2 ? 0.3f : -0.3f);
    }
    const auto k = maptiles::SimplifyScreen(xs, ys, maptiles::kMinSegmentPx);
    ATF_REQUIRE(k.size() >= 6 && k.size() <= 9);
    ATF_REQUIRE_EQ(k.front(), 0u);
    ATF_REQUIRE_EQ(k.back(), 99u);
}

ATF_TEST_CASE_WITHOUT_HEAD(spread_points_all_kept);
ATF_TEST_CASE_BODY(spread_points_all_kept)
{
    const std::vector<float> xs = {0, 10, 20, 30}, ys = {0, 5, 0, 5};
    ATF_REQUIRE_EQ(maptiles::SimplifyScreen(xs, ys, 1.5f).size(), 4u);
}

ATF_TEST_CASE_WITHOUT_HEAD(degenerate_inputs);
ATF_TEST_CASE_BODY(degenerate_inputs)
{
    ATF_REQUIRE(maptiles::SimplifyScreen({}, {}, 1.5f).empty());
    ATF_REQUIRE_EQ(maptiles::SimplifyScreen({5}, {5}, 1.5f).size(), 1u);
    ATF_REQUIRE_EQ(maptiles::SimplifyScreen({5, 5.1f}, {5, 5}, 1.5f).size(), 2u);  // ends kept
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, sub_pixel_zigzag_collapses);
    ATF_ADD_TEST_CASE(tcs, spread_points_all_kept);
    ATF_ADD_TEST_CASE(tcs, degenerate_inputs);
}
