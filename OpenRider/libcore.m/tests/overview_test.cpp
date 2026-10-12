#include <atf-c++.hpp>

#include "core/library.h"
#include "core/overview.h"

#include <cmath>
#include <cstdio>
#include <limits>
#include <set>

namespace {

// 1 km north from 45.9N, 6.6E, one point per metre.
std::vector<or_point> line_km()
{
    std::vector<or_point> pts;
    for (int i = 0; i <= 1000; ++i) {
        or_point p{};
        p.t_s = i;
        p.lat = 45.9 + i * 9e-6;
        p.lon = 6.6;
        pts.push_back(p);
    }
    return pts;
}

} // namespace

ATF_TEST_CASE_WITHOUT_HEAD(thinning_keeps_shape_and_ends);
ATF_TEST_CASE_BODY(thinning_keeps_shape_and_ends)
{
    const auto pts = line_km();
    const auto t = core::thin_track("1", pts, 30.0);
    ATF_REQUIRE(t.lat.size() >= 33 && t.lat.size() <= 36);  // ~1000 m / 30 m
    ATF_REQUIRE_EQ(t.lat.front(), pts.front().lat);
    ATF_REQUIRE_EQ(t.lat.back(), pts.back().lat);
    ATF_REQUIRE(std::fabs(t.north - pts.back().lat) < 1e-12);
}

ATF_TEST_CASE_WITHOUT_HEAD(partly_within_the_view);
ATF_TEST_CASE_BODY(partly_within_the_view)
{
    const auto t = core::thin_track("1", line_km(), 30.0);
    ATF_REQUIRE(core::track_in_box(t, 6.5, 45.904, 6.7, 46.0));   // northern half shown
    ATF_REQUIRE(!core::track_in_box(t, 6.7, 45.0, 6.8, 46.0));    // view east of it
    ATF_REQUIRE(!core::track_in_box(core::OverviewTrack{}, -180, -90, 180, 90));
}

ATF_TEST_CASE_WITHOUT_HEAD(long_dates_by_language);
ATF_TEST_CASE_BODY(long_dates_by_language)
{
    ATF_REQUIRE_EQ(core::long_date("2026-10-12 15:53:03", "en"), "12 October 2026");
    ATF_REQUIRE_EQ(core::long_date("2026-08-03 08:00:00", "fr"), "3 août 2026");
    ATF_REQUIRE_EQ(core::long_date("garbage", "en"), "garbage");
}

ATF_TEST_CASE_WITHOUT_HEAD(list_restricted_to_ids);
ATF_TEST_CASE_BODY(list_restricted_to_ids)
{
    std::vector<core::Outing> v(3);
    for (int i = 0; i < 3; ++i) {
        std::snprintf(v[i].raw.id, sizeof v[i].raw.id, "%d", i);
        std::snprintf(v[i].raw.start_local, sizeof v[i].raw.start_local, "2026-01-0%d 10:00:00", i + 1);
    }
    const std::set<std::string> visible = {"0", "2"};
    core::OutingFilter f;
    f.only_ids = &visible;
    ATF_REQUIRE_EQ(core::select_outings(v, f, core::SortKey::Date, true).size(), 2u);
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, thinning_keeps_shape_and_ends);
    ATF_ADD_TEST_CASE(tcs, partly_within_the_view);
    ATF_ADD_TEST_CASE(tcs, long_dates_by_language);
    ATF_ADD_TEST_CASE(tcs, list_restricted_to_ids);
}
