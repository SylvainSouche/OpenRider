#include <atf-c++.hpp>

#include <openrider/sdk/motion.h>
#include <openrider/sdk/stats.h>

#include <cmath>

namespace {

// Walk north at 1.5 m/s for 20 min, stand still 7 min with +/-2 m GPS
// jitter, walk on 20 min. One sample every 2 s. ~1 m of latitude = 9e-6 deg.
std::vector<or_point> WalkWithPause(double jitter = 2.0)
{
    std::vector<or_point> pts;
    double lat = 45.9, t = 0.0, alt = 1000.0;
    auto push = [&](double dlat_m, double jitter_m) {
        or_point p{};
        p.t_s = t;
        lat += dlat_m * 9e-6;
        p.lat = lat + jitter_m * 9e-6;
        p.lon = 6.6;
        p.alt_m = alt;
        p.speed_ms = dlat_m / 2.0;
        p.dist_m = std::numeric_limits<double>::quiet_NaN();
        pts.push_back(p);
        t += 2.0;
    };
    for (int i = 0; i < 600; ++i)
        push(3.0, 0.0);
    for (int i = 0; i < 210; ++i)
        push(0.0, (i % 2 ? jitter : -jitter));
    for (int i = 0; i < 600; ++i)
        push(3.0, 0.0);
    return pts;
}

} // namespace

ATF_TEST_CASE_WITHOUT_HEAD(percentile_ignores_nan);
ATF_TEST_CASE_BODY(percentile_ignores_nan)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    ATF_REQUIRE_EQ(openrider::sdk::Percentile({5, nan, 1, 3, 2, 4, 6, 7, 8, 9, 10}, 10.0), 2.0);
    ATF_REQUIRE(std::isnan(openrider::sdk::Percentile({nan}, 10.0)));
}

ATF_TEST_CASE_WITHOUT_HEAD(window_scales_with_activity);
ATF_TEST_CASE_BODY(window_scales_with_activity)
{
    // Moving 1.5 m/s most of the time, so the 10th percentile is the walk
    // speed or the pause; the window is 50 m at that speed (finite, > 0).
    const double w = openrider::sdk::StillWindowS(WalkWithPause(), openrider::sdk::MotionParams{});
    ATF_REQUIRE(!std::isnan(w) && w > 0.0);
}

ATF_TEST_CASE_WITHOUT_HEAD(pause_is_not_moving);
ATF_TEST_CASE_BODY(pause_is_not_moving)
{
    const auto pts = WalkWithPause();
    const auto still = openrider::sdk::NotMovingMask(pts, openrider::sdk::MotionParams{});
    int in_pause = 0, in_walk = 0;
    for (size_t i = 0; i < pts.size(); ++i) {
        if (i >= 620 && i < 790)
            in_pause += still[i];
        if (i < 580 || i >= 830)
            in_walk += still[i];
    }
    ATF_REQUIRE(in_pause > 160);  // nearly all of the pause
    ATF_REQUIRE_EQ(in_walk, 0);   // never while walking
}

ATF_TEST_CASE_WITHOUT_HEAD(stats_moving_time_excludes_pause);
ATF_TEST_CASE_BODY(stats_moving_time_excludes_pause)
{
    const auto st = openrider::sdk::ComputeStats(WalkWithPause(), openrider::sdk::MotionParams{});
    ATF_REQUIRE(std::fabs(st.duration_s - 2818.0) < 1e-6);
    ATF_REQUIRE(st.moving_s < st.duration_s - 360.0);  // most of the 7 min pause removed
    ATF_REQUIRE(st.moving_s > 2350.0);
    // Raw jitter (+/-2 m every 2 s) reads as 2 m/s: faster than the walk.
    // That is what the jitter filter removes; on the clean stretch alone the
    // maximum is the walking speed.
    ATF_REQUIRE(st.max_speed_ms > 1.9);
    const auto pts = WalkWithPause();
    const std::vector<or_point> walk(pts.begin(), pts.begin() + 600);
    ATF_REQUIRE(std::fabs(openrider::sdk::ComputeStats(walk, openrider::sdk::MotionParams{}).max_speed_ms - 1.5) < 0.01);
}

// The recorded speed decides: a stop whose positions wander by +/-8 m (more
// than the positional rule tolerates) is still not moving, and the moving
// time is the time of the points recorded above 0.3 m/s.
ATF_TEST_CASE_WITHOUT_HEAD(recorded_speed_decides);
ATF_TEST_CASE_BODY(recorded_speed_decides)
{
    const auto pts = WalkWithPause(8.0);
    const auto still = openrider::sdk::NotMovingMask(pts, openrider::sdk::MotionParams{});
    for (size_t i = 0; i < pts.size(); ++i)
        ATF_REQUIRE_EQ(still[i], i >= 600 && i < 810);
    const auto st = openrider::sdk::ComputeStats(pts, openrider::sdk::MotionParams{});
    ATF_REQUIRE(std::fabs(st.moving_s - 1199.0 * 2.0) < 1e-6);  // every walking point but the first
}

// Without recorded speed, the positional rule finds the pause.
ATF_TEST_CASE_WITHOUT_HEAD(without_speed_falls_back_on_positions);
ATF_TEST_CASE_BODY(without_speed_falls_back_on_positions)
{
    auto pts = WalkWithPause();
    for (or_point &p : pts)
        p.speed_ms = std::numeric_limits<double>::quiet_NaN();
    const auto still = openrider::sdk::NotMovingMask(pts, openrider::sdk::MotionParams{});
    int in_pause = 0, in_walk = 0;
    for (size_t i = 0; i < pts.size(); ++i) {
        if (i >= 620 && i < 790)
            in_pause += still[i];
        if (i < 580 || i >= 830)
            in_walk += still[i];
    }
    ATF_REQUIRE(in_pause > 160);
    ATF_REQUIRE_EQ(in_walk, 0);
}

ATF_TEST_CASE_WITHOUT_HEAD(ascent_with_hysteresis);
ATF_TEST_CASE_BODY(ascent_with_hysteresis)
{
    std::vector<or_point> pts;
    const double alts[] = {1000, 1001, 999, 1001, 1010, 1020, 1019, 1021, 1005};
    for (int i = 0; i < 9; ++i) {
        or_point p{};
        p.t_s = i;
        p.alt_m = alts[i];
        p.lat = p.lon = std::numeric_limits<double>::quiet_NaN();
        p.dist_m = i;
        pts.push_back(p);
    }
    const auto st = openrider::sdk::ComputeStats(pts, openrider::sdk::MotionParams{});
    ATF_REQUIRE(std::fabs(st.ascent_m - 20.0) < 1e-6);   // +/-2 m noise ignored
    ATF_REQUIRE(std::fabs(st.descent_m - 15.0) < 1e-6);
    ATF_REQUIRE_EQ(st.max_alt_m, 1021.0);
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, percentile_ignores_nan);
    ATF_ADD_TEST_CASE(tcs, window_scales_with_activity);
    ATF_ADD_TEST_CASE(tcs, pause_is_not_moving);
    ATF_ADD_TEST_CASE(tcs, stats_moving_time_excludes_pause);
    ATF_ADD_TEST_CASE(tcs, recorded_speed_decides);
    ATF_ADD_TEST_CASE(tcs, without_speed_falls_back_on_positions);
    ATF_ADD_TEST_CASE(tcs, ascent_with_hysteresis);
}
