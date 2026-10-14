// Rolling-window statistics and rests on synthetic recorded points.

#include <atf-c++.hpp>

#include <openrider/sdk/plane_track.h>
#include <openrider/sdk/rests.h>
#include <openrider/sdk/window_stats.h>

#include <cmath>

namespace {

constexpr double kLat0 = 45.9, kLon0 = 6.6;
constexpr double kKx = 111320.0 * 0.69591, kKy = 110540.0;

// Recorded points, one a second, built by legs.
struct Builder {
    std::vector<or_point> points;
    double x = 0.0, y = 0.0, t = 0.0;

    void Push(double speed, double jitter_x = 0.0, double jitter_y = 0.0)
    {
        or_point p{};
        p.t_s = t;
        p.lat = kLat0 + (y + jitter_y) / kKy;
        p.lon = kLon0 + (x + jitter_x) / kKx;
        p.speed_ms = speed;
        p.alt_m = p.dist_m = p.hr = p.cadence = p.temp_c = std::nan("");
        points.push_back(p);
        t += 1.0;
    }
    // North at `speed` for `seconds`.
    void Walk(double speed, int seconds)
    {
        for (int i = 0; i < seconds; ++i) {
            y += speed;
            Push(speed);
        }
    }
    // A pause of `seconds`: mostly still with +/-4 m of wander, and every
    // other minute a 20 s stroll round a 10 m circle (recorded at 1 m/s).
    void Picnic(int seconds)
    {
        for (int i = 0; i < seconds; ++i) {
            if ((i / 60) % 2 == 1 && i % 60 < 20) {
                const double a = (i % 60) / 20.0 * 6.283;
                Push(1.0, 10.0 * std::cos(a) - 10.0, 10.0 * std::sin(a));
            } else {
                Push(0.0, 4.0 * std::sin(i * 1.7), 4.0 * std::cos(i * 2.3));
            }
        }
    }
};

} // namespace

ATF_TEST_CASE_WITHOUT_HEAD(window_statistics_of_a_stop);
ATF_TEST_CASE_BODY(window_statistics_of_a_stop)
{
    Builder b;
    b.Walk(2.0, 120);
    b.Picnic(300);
    const auto stats = openrider::sdk::RollingStats(b.points, 60.0);
    const auto &walking = stats[60], &stopped = stats[120 + 240];  // the latter in a still minute of the picnic
    ATF_REQUIRE(std::fabs(walking.displacement_m - 120.0) < 3.0);
    ATF_REQUIRE(std::fabs(walking.speed_median - 2.0) < 1e-9);
    ATF_REQUIRE(walking.stopped_fraction < 0.01);
    ATF_REQUIRE(stopped.radius_m < 8.0);
    ATF_REQUIRE(stopped.stopped_fraction > 0.95);
    ATF_REQUIRE(stopped.displacement_m < 0.5 * stopped.path_m);  // wandering goes nowhere
}

ATF_TEST_CASE_WITHOUT_HEAD(picnic_with_strolls_is_one_rest);
ATF_TEST_CASE_BODY(picnic_with_strolls_is_one_rest)
{
    Builder b;
    b.Walk(1.5, 300);
    const size_t start = b.points.size();
    b.Picnic(1200);
    const size_t end = b.points.size() - 1;
    b.Walk(1.5, 300);
    const auto rests = openrider::sdk::RestIntervals(b.points, openrider::sdk::RestParams{});
    ATF_REQUIRE_EQ(rests.size(), 1u);
    ATF_REQUIRE(rests[0].first <= start + 5 && rests[0].last + 5 >= end);
    ATF_REQUIRE(rests[0].duration_s > 1150.0);
    const double off = std::hypot((rests[0].lon - kLon0) * kKx - (-2.0), (rests[0].lat - kLat0) * kKy - 450.0);
    ATF_REQUIRE(off < 10.0);  // the picnic is at 450 m north
}

ATF_TEST_CASE_WITHOUT_HEAD(slow_climb_is_no_rest);
ATF_TEST_CASE_BODY(slow_climb_is_no_rest)
{
    // 0.4 m/s straight on for 20 min (above the 0.3 m/s at which the device
    // itself counts a stop): within 30 m of its centre over 2 min, but going
    // somewhere -- progress, not a pause.
    Builder b;
    b.Walk(0.4, 1200);
    ATF_REQUIRE(openrider::sdk::RestIntervals(b.points, openrider::sdk::RestParams{}).empty());
    openrider::sdk::RestParams radius_only;
    radius_only.max_displacement_ratio = std::nan("");
    // Without the displacement criterion it would rest, but a rest cannot
    // drift: pieces spanning about four times the radius at most.
    for (const auto &rest : openrider::sdk::RestIntervals(b.points, radius_only))
        ATF_REQUIRE(std::fabs(b.points[rest.last].lat - b.points[rest.first].lat) * kKy <= 4.0 * 30.0 + 5.0);
}

ATF_TEST_CASE_WITHOUT_HEAD(rests_left_out_of_plane_tracks);
ATF_TEST_CASE_BODY(rests_left_out_of_plane_tracks)
{
    Builder b;
    b.Walk(1.5, 300);
    b.Picnic(600);
    b.Walk(1.5, 300);
    openrider::sdk::RecordedTrack track{"a", b.points};
    const auto rests = openrider::sdk::RestIntervals(track.points, openrider::sdk::RestParams{});
    const auto plane = openrider::sdk::ToPlane(track, openrider::sdk::LocalProjection(kLon0, kLat0), rests);
    ATF_REQUIRE_EQ(plane.breaks.size(), 1u);
    const size_t b0 = plane.breaks[0];
    ATF_REQUIRE(b0 > 250 && b0 < 310);                                         // after the first walk
    ATF_REQUIRE(plane.size() < 640u);                                           // the pause is out
    ATF_REQUIRE(std::fabs(plane.points[b0].y - plane.points[b0 - 1].y) < 15.0);  // resumes where it left
}

ATF_TEST_CASE_WITHOUT_HEAD(rest_places_group_nearby_rests);
ATF_TEST_CASE_BODY(rest_places_group_nearby_rests)
{
    std::vector<openrider::sdk::Rest> rests = {{0, 1, kLon0, kLat0, 300}, {0, 1, kLon0 + 10 / kKx, kLat0, 300},
                                               {0, 1, kLon0, kLat0 + 200 / kKy, 300}};
    const auto places = openrider::sdk::RestPlaces(rests, 30.0);
    ATF_REQUIRE_EQ(places.size(), 2u);
    ATF_REQUIRE_EQ(places[0].rests, 2);
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, window_statistics_of_a_stop);
    ATF_ADD_TEST_CASE(tcs, picnic_with_strolls_is_one_rest);
    ATF_ADD_TEST_CASE(tcs, slow_climb_is_no_rest);
    ATF_ADD_TEST_CASE(tcs, rests_left_out_of_plane_tracks);
    ATF_ADD_TEST_CASE(tcs, rest_places_group_nearby_rests);
}
