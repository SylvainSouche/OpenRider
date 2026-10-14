// The heat-map plugin through its ABI, as the application loads it.

#include <atf-c++.hpp>

#include <openrider/plugin.h>
#include <openrider/sdk/overview.h>
#include <openrider/sdk/segment_exchange.h>
#include <openrider/segments.h>

#include <cmath>
#include <string>
#include <vector>

extern "C" const or_plugin_info *openrider_plugin(void);

namespace {

const or_segments_vtable *Vt()
{
    return static_cast<const or_segments_vtable *>(openrider_plugin()->vtable);
}

// A straight track northwards, `length_m` long, a point every 10 m.
openrider::sdk::OverviewTrack Straight(const std::string &id, double length_m, double offset_m)
{
    openrider::sdk::OverviewTrack track;
    track.id = id;
    for (double d = 0.0; d <= length_m; d += 10.0) {
        track.lat.push_back(45.9 + d / 110540.0);
        track.lon.push_back(6.6 + offset_m / 77000.0);
    }
    return track;
}

// The recorded points of a test line: one a second along its positions.
openrider::sdk::RecordedTrack Recorded(const openrider::sdk::OverviewTrack &line)
{
    openrider::sdk::RecordedTrack track{line.id, {}};
    for (size_t i = 0; i < line.lat.size(); ++i) {
        or_point p{};
        p.t_s = static_cast<double>(i);
        p.lat = line.lat[i];
        p.lon = line.lon[i];
        p.alt_m = p.dist_m = p.speed_ms = p.hr = p.cadence = p.temp_c = std::nan("");
        track.points.push_back(p);
    }
    return track;
}

std::vector<double> Defaults()
{
    std::vector<double> values;
    for (size_t p = 0; p < Vt()->param_count; ++p)
        values.push_back(Vt()->params[p].default_value);
    return values;
}

} // namespace

ATF_TEST_CASE_WITHOUT_HEAD(descriptor_is_a_segments_plugin);
ATF_TEST_CASE_BODY(descriptor_is_a_segments_plugin)
{
    const or_plugin_info *info = openrider_plugin();
    ATF_REQUIRE(info && info->vtable && info->name);
    ATF_REQUIRE_EQ(info->api_version, static_cast<uint32_t>(OR_PLUGIN_API_VERSION));
    ATF_REQUIRE_EQ(info->kind, static_cast<uint32_t>(OR_PLUGIN_SEGMENTS));
    ATF_REQUIRE(Vt()->param_count >= 1);
    ATF_REQUIRE_EQ(std::string(Vt()->params[0].id), "trace_width_m");
}

ATF_TEST_CASE_WITHOUT_HEAD(two_tracks_share_a_segment);
ATF_TEST_CASE_BODY(two_tracks_share_a_segment)
{
    const std::vector<openrider::sdk::RecordedTrack> tracks = {Recorded(Straight("a", 600, 0)), Recorded(Straight("b", 600, 3))};
    const auto result = openrider::sdk::RunSegments(Vt(), tracks, Defaults(), nullptr, nullptr);
    ATF_REQUIRE(result.Ok());
    ATF_REQUIRE_EQ(result.value.routes.size(), 2u);
    bool shared = false;
    for (const openrider::sdk::NetworkSegment &segment : result.value.segments)
        shared = shared || segment.Shared();
    ATF_REQUIRE(shared);
    ATF_REQUIRE_EQ(result.value.routes[0].id, "a");
    // Each route's steps cover its track, one after the other.
    for (size_t r = 0; r < tracks.size(); ++r) {
        const auto &steps = result.value.routes[r].steps;
        ATF_REQUIRE(!steps.empty());
        ATF_REQUIRE_EQ(steps.front().first, 0u);
        ATF_REQUIRE_EQ(steps.back().last, tracks[r].points.size() - 1);
        for (size_t k = 1; k < steps.size(); ++k)
            ATF_REQUIRE_EQ(steps[k].first, steps[k - 1].last);
    }
}

ATF_TEST_CASE_WITHOUT_HEAD(cancel_stops_early);
ATF_TEST_CASE_BODY(cancel_stops_early)
{
    const std::vector<openrider::sdk::RecordedTrack> tracks = {Recorded(Straight("a", 600, 0)), Recorded(Straight("b", 600, 3))};
    const auto result = openrider::sdk::RunSegments(Vt(), tracks, Defaults(), [](void *) { return 1; }, nullptr);
    ATF_REQUIRE(!result.Ok());  // stopped early: no network, not a success
    ATF_REQUIRE(result.error.find("cancelled") != std::string::npos);
}

ATF_TEST_CASE_WITHOUT_HEAD(malformed_input_refused);
ATF_TEST_CASE_BODY(malformed_input_refused)
{
    std::vector<double> values = Defaults();
    or_segment_network *out = reinterpret_cast<or_segment_network *>(1);  // must be reset
    char err[128] = "";
    or_segment_track no_points{"a", 2, nullptr};
    ATF_REQUIRE(Vt()->build(&no_points, 1, values.data(), nullptr, nullptr, &out, err, sizeof err) != 0);
    ATF_REQUIRE(out == nullptr);
    ATF_REQUIRE(std::string(err).find("no points array") != std::string::npos);
    const openrider::sdk::RecordedTrack line = Recorded(Straight("a", 20, 0));
    std::vector<or_point> infinite = line.points;
    infinite[1].lat = INFINITY;
    or_segment_track not_finite{"a", infinite.size(), infinite.data()};
    ATF_REQUIRE(Vt()->build(&not_finite, 1, values.data(), nullptr, nullptr, &out, err, sizeof err) != 0);
    ATF_REQUIRE(out == nullptr);
    std::vector<or_point> unlocated = line.points;
    unlocated[1].lat = unlocated[1].lon = std::nan("");  // no position: accepted
    or_segment_track missing_position{"a", unlocated.size(), unlocated.data()};
    ATF_REQUIRE(openrider::sdk::TracksProblem(&missing_position, 1).empty());
    or_segment_track good{"a", line.points.size(), line.points.data()};
    values[0] = Vt()->params[0].max_value + 1.0;
    ATF_REQUIRE(Vt()->build(&good, 1, values.data(), nullptr, nullptr, &out, err, sizeof err) != 0);
    ATF_REQUIRE(std::string(err).find("bounds") != std::string::npos);
    values[0] = std::nan("");
    ATF_REQUIRE(Vt()->build(&good, 1, values.data(), nullptr, nullptr, &out, err, sizeof err) != 0);
    ATF_REQUIRE(out == nullptr);
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, descriptor_is_a_segments_plugin);
    ATF_ADD_TEST_CASE(tcs, two_tracks_share_a_segment);
    ATF_ADD_TEST_CASE(tcs, cancel_stops_early);
    ATF_ADD_TEST_CASE(tcs, malformed_input_refused);
}
