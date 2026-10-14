#include <atf-c++.hpp>

#include "core/library.h"
#include "core/outing_selection.h"
#include "core/long_date.h"
#include "core/overview_loader.h"

#include <openrider/sdk/overview.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <limits>
#include <set>
#include <thread>

namespace {

// 1 km north from 45.9N, 6.6E, one point per metre.
std::vector<or_point> LineKm()
{
    std::vector<or_point> pts;
    for (int i = 0; i <= 1000; ++i) {
        or_point p{};
        p.t_s = i;
        p.lat = 45.9 + i * 9e-6;
        p.lon = 6.6;
        p.speed_ms = 1.0;  // 1 m a second
        pts.push_back(p);
    }
    return pts;
}

// A fake input plugin: every outing is LineKm().
int FakePoints(void *, const char *, or_point_cb cb, void *user, char *, size_t)
{
    for (const or_point &point : LineKm())
        cb(user, &point);
    return 0;
}
const or_input_vtable kInput = {sizeof(or_input_vtable), nullptr, nullptr, nullptr, FakePoints};

// Waits for a load to end, then takes its tracks.
std::vector<openrider::sdk::OverviewTrack> Finish(core::OverviewLoader &loader)
{
    for (int i = 0; i < 2000 && loader.Loading(); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    std::vector<openrider::sdk::OverviewTrack> tracks;
    loader.Take(tracks);
    return tracks;
}

} // namespace

ATF_TEST_CASE_WITHOUT_HEAD(thinning_keeps_shape_and_ends);
ATF_TEST_CASE_BODY(thinning_keeps_shape_and_ends)
{
    const auto pts = LineKm();
    const auto t = openrider::sdk::ThinTrack("1", pts, 30.0);
    ATF_REQUIRE(t.lat.size() >= 33 && t.lat.size() <= 36);  // ~1000 m / 30 m
    ATF_REQUIRE_EQ(t.lat.front(), pts.front().lat);
    ATF_REQUIRE_EQ(t.lat.back(), pts.back().lat);
    ATF_REQUIRE(std::fabs(t.north - pts.back().lat) < 1e-12);
}

ATF_TEST_CASE_WITHOUT_HEAD(partly_within_the_view);
ATF_TEST_CASE_BODY(partly_within_the_view)
{
    const auto t = openrider::sdk::ThinTrack("1", LineKm(), 30.0);
    ATF_REQUIRE(openrider::sdk::TrackInBox(t, 6.5, 45.904, 6.7, 46.0));   // northern half shown
    ATF_REQUIRE(!openrider::sdk::TrackInBox(t, 6.7, 45.0, 6.8, 46.0));    // view east of it
    ATF_REQUIRE(!openrider::sdk::TrackInBox(openrider::sdk::OverviewTrack{}, -180, -90, 180, 90));
}

ATF_TEST_CASE_WITHOUT_HEAD(long_dates_by_language);
ATF_TEST_CASE_BODY(long_dates_by_language)
{
    ATF_REQUIRE_EQ(core::LongDate("2026-10-12 15:53:03", "en"), "12 October 2026");
    ATF_REQUIRE_EQ(core::LongDate("2026-08-03 08:00:00", "fr"), "3 août 2026");
    ATF_REQUIRE_EQ(core::LongDate("garbage", "en"), "garbage");
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
    ATF_REQUIRE_EQ(core::SelectOutings(v, f, core::SortKey::Date, true).size(), 2u);
}

ATF_TEST_CASE_WITHOUT_HEAD(loader_restarts_after_stop);
ATF_TEST_CASE_BODY(loader_restarts_after_stop)
{
    core::OverviewLoader loader;
    loader.Start(&kInput, nullptr, {"a", "b"});
    ATF_REQUIRE_EQ(Finish(loader).size(), 2u);
    loader.Stop();
    loader.Start(&kInput, nullptr, {"c", "d", "e"});
    ATF_REQUIRE_EQ(Finish(loader).size(), 3u);  // a previous Stop() does not end the new load
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, loader_restarts_after_stop);
    ATF_ADD_TEST_CASE(tcs, thinning_keeps_shape_and_ends);
    ATF_ADD_TEST_CASE(tcs, partly_within_the_view);
    ATF_ADD_TEST_CASE(tcs, long_dates_by_language);
    ATF_ADD_TEST_CASE(tcs, list_restricted_to_ids);
}
