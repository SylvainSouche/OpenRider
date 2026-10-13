// The heat-map plugin through its ABI, as the application loads it.

#include <atf-c++.hpp>

#include <openrider/plugin.h>
#include <openrider/sdk/segment_exchange.h>
#include <openrider/segments.h>

#include <cmath>
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
    const std::vector<openrider::sdk::OverviewTrack> tracks = {Straight("a", 600, 0), Straight("b", 600, 3)};
    const auto result = openrider::sdk::RunSegments(Vt(), tracks, Defaults(), nullptr, nullptr);
    ATF_REQUIRE(result.Ok());
    ATF_REQUIRE_EQ(result.value.routes.size(), 2u);
    bool shared = false;
    for (const openrider::sdk::NetworkSegment &segment : result.value.segments)
        shared = shared || segment.Shared();
    ATF_REQUIRE(shared);
    ATF_REQUIRE_EQ(result.value.routes[0].id, "a");
}

ATF_TEST_CASE_WITHOUT_HEAD(cancel_stops_early);
ATF_TEST_CASE_BODY(cancel_stops_early)
{
    const std::vector<openrider::sdk::OverviewTrack> tracks = {Straight("a", 600, 0), Straight("b", 600, 3)};
    const auto result = openrider::sdk::RunSegments(Vt(), tracks, Defaults(), [](void *) { return 1; }, nullptr);
    ATF_REQUIRE(result.Ok());  // stopped early: an incomplete network the caller discards
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, descriptor_is_a_segments_plugin);
    ATF_ADD_TEST_CASE(tcs, two_tracks_share_a_segment);
    ATF_ADD_TEST_CASE(tcs, cancel_stops_early);
}
