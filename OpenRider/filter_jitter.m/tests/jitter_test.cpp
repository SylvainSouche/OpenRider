#include <atf-c++.hpp>

#include <openrider/filter.h>
#include <openrider/plugin.h>

#include "core/stats.h"

#include <cmath>
#include <limits>
#include <vector>

extern "C" const or_plugin_info *openrider_plugin(void);

namespace {

const or_filter_vtable *Vt()
{
    return static_cast<const or_filter_vtable *>(openrider_plugin()->vtable);
}

// Walk 1.5 m/s, 7 min pause with +/-2 m jitter, walk on; one GPS spike of
// 400 m during the second walk. One sample every 2 s.
std::vector<or_point> Outing()
{
    std::vector<or_point> pts;
    double lat = 45.9, t = 0.0;
    auto push = [&](double dlat_m, double jitter_m) {
        or_point p{};
        p.t_s = t;
        lat += dlat_m * 9e-6;
        p.lat = lat + jitter_m * 9e-6;
        p.lon = 6.6;
        p.alt_m = 1000.0;
        p.speed_ms = dlat_m / 2.0;
        p.dist_m = std::numeric_limits<double>::quiet_NaN();
        pts.push_back(p);
        t += 2.0;
    };
    for (int i = 0; i < 600; ++i)
        push(3.0, 0.0);
    for (int i = 0; i < 210; ++i)
        push(0.0, i % 2 ? 2.0 : -2.0);
    for (int i = 0; i < 600; ++i)
        push(3.0, i == 300 ? 400.0 : 0.0);
    return pts;
}

std::vector<double> Defaults()
{
    std::vector<double> v;
    for (size_t i = 0; i < Vt()->param_count; ++i)
        v.push_back(Vt()->params[i].default_value);
    return v;
}

std::vector<or_point> Filtered(const std::vector<or_point> &pts, std::vector<unsigned char> &keep)
{
    keep.assign(pts.size(), 0);
    char err[128] = "";
    const std::vector<double> v = Defaults();
    ATF_REQUIRE_EQ(Vt()->apply_track(pts.data(), pts.size(), v.data(), keep.data(), err, sizeof err), 0);
    std::vector<or_point> out;
    for (size_t i = 0; i < pts.size(); ++i)
        if (keep[i])
            out.push_back(pts[i]);
    return out;
}

} // namespace

ATF_TEST_CASE_WITHOUT_HEAD(descriptor_and_parameters);
ATF_TEST_CASE_BODY(descriptor_and_parameters)
{
    ATF_REQUIRE_EQ(openrider_plugin()->kind, static_cast<uint32_t>(OR_PLUGIN_FILTER));
    ATF_REQUIRE_EQ(Vt()->kind, static_cast<uint32_t>(OR_FILTER_TRACK));
    ATF_REQUIRE_EQ(Vt()->param_count, 4u);
    ATF_REQUIRE_EQ(std::string(Vt()->params[0].id), "ref_distance_m");
    ATF_REQUIRE_EQ(Vt()->params[0].default_value, 50.0);
}

ATF_TEST_CASE_WITHOUT_HEAD(pause_jitter_removed_ends_kept);
ATF_TEST_CASE_BODY(pause_jitter_removed_ends_kept)
{
    const auto pts = Outing();
    std::vector<unsigned char> keep;
    Filtered(pts, keep);
    int kept_in_pause = 0;
    for (size_t i = 620; i < 790; ++i)
        kept_in_pause += keep[i];
    ATF_REQUIRE(kept_in_pause <= 4);
    ATF_REQUIRE_EQ(keep.front(), 1);
    ATF_REQUIRE_EQ(keep.back(), 1);
    for (size_t i = 0; i < 580; ++i)
        ATF_REQUIRE_EQ(keep[i], 1);  // the walk itself is untouched
}

ATF_TEST_CASE_WITHOUT_HEAD(spike_removed);
ATF_TEST_CASE_BODY(spike_removed)
{
    const auto pts = Outing();
    std::vector<unsigned char> keep;
    Filtered(pts, keep);
    ATF_REQUIRE_EQ(keep[810 + 300], 0);
}

ATF_TEST_CASE_WITHOUT_HEAD(stats_after_filter_are_clean);
ATF_TEST_CASE_BODY(stats_after_filter_are_clean)
{
    const auto pts = Outing();
    std::vector<unsigned char> keep;
    const auto clean = Filtered(pts, keep);
    const auto raw = core::ComputeStats(pts, core::MotionParams{});
    const auto st = core::ComputeStats(clean, core::MotionParams{});
    ATF_REQUIRE(raw.max_speed_ms > 1.9);                  // jitter and spike inflate it
    ATF_REQUIRE(std::fabs(st.max_speed_ms - 1.5) < 0.05); // back to the walk
    ATF_REQUIRE(std::fabs(st.distance_m - 3600.0) < 20.0);
    ATF_REQUIRE(std::fabs(st.duration_s - raw.duration_s) < 1e-6);  // pause ends kept
    ATF_REQUIRE(st.moving_s < st.duration_s - 400.0);     // the pause is still not moving
    ATF_REQUIRE(std::fabs(st.moving_s - raw.moving_s) < 60.0);
    // With the raw points given, moving time is exactly the raw one: the
    // filter cannot make the user move longer.
    const auto anchored = core::ComputeStats(clean, core::MotionParams{}, &pts);
    ATF_REQUIRE_EQ(anchored.moving_s, raw.moving_s);
    ATF_REQUIRE(anchored.distance_m < raw.distance_m);
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, descriptor_and_parameters);
    ATF_ADD_TEST_CASE(tcs, pause_jitter_removed_ends_kept);
    ATF_ADD_TEST_CASE(tcs, spike_removed);
    ATF_ADD_TEST_CASE(tcs, stats_after_filter_are_clean);
}
