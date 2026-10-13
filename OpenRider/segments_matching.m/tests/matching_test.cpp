// Track matching on synthetic tracks, and through the plugin ABI.

#include <atf-c++.hpp>

#include <openrider/plugin.h>
#include <openrider/segments.h>

#include "matching/matching.h"

#include <openrider/sdk/segment_exchange.h>

#include <cmath>
#include <vector>

extern "C" const or_plugin_info *openrider_plugin(void);

namespace {

constexpr double kLat0 = 45.9, kLon0 = 6.6;
constexpr double kKx = 111320.0 * 0.69591;  // cos(45.9 deg)
constexpr double kKy = 110540.0;

// A track through metric corners (x east, y north), a point every 10 m,
// with +/- 2 m of noise.
openrider::sdk::OverviewTrack Track(const std::string &id, const std::vector<std::pair<double, double>> &corners, unsigned seed)
{
    openrider::sdk::OverviewTrack track;
    track.id = id;
    for (size_t c = 1; c < corners.size(); ++c) {
        const double x0 = corners[c - 1].first, y0 = corners[c - 1].second;
        const double x1 = corners[c].first, y1 = corners[c].second;
        const int n = std::max(1, static_cast<int>(std::hypot(x1 - x0, y1 - y0) / 10.0));
        for (int i = 0; i < n; ++i) {
            seed = seed * 1103515245u + 12345u;
            const double noise = ((seed >> 16) % 400) / 100.0 - 2.0;
            track.lat.push_back(kLat0 + (y0 + (y1 - y0) * i / n + noise) / kKy);
            track.lon.push_back(kLon0 + (x0 + (x1 - x0) * i / n) / kKx);
        }
    }
    track.lat.push_back(kLat0 + corners.back().second / kKy);
    track.lon.push_back(kLon0 + corners.back().first / kKx);
    return track;
}

// Metric position of a network node.
std::pair<double, double> At(const openrider::sdk::NetworkNode &node)
{
    return {(node.lon - kLon0) * kKx, (node.lat - kLat0) * kKy};
}

void RequireConnected(const openrider::sdk::SegmentNetwork &net)
{
    for (const openrider::sdk::NetworkRoute &route : net.routes)
        for (size_t i = 1; i < route.steps.size(); ++i) {
            const openrider::sdk::NetworkSegment &a = net.segments[static_cast<size_t>(route.steps[i - 1].segment)];
            const openrider::sdk::NetworkSegment &b = net.segments[static_cast<size_t>(route.steps[i].segment)];
            ATF_REQUIRE_EQ(route.steps[i - 1].forward ? a.to : a.from, route.steps[i].forward ? b.from : b.to);
        }
}

} // namespace

ATF_TEST_CASE_WITHOUT_HEAD(shared_stretch_and_junctions);
ATF_TEST_CASE_BODY(shared_stretch_and_junctions)
{
    // a and b share y = 0 from x = 0 to 1000, arriving and leaving apart.
    const std::vector<openrider::sdk::OverviewTrack> tracks = {
        Track("a", {{-300, 400}, {0, 0}, {1000, 0}, {1300, 400}}, 7),
        Track("b", {{-300, -400}, {0, 0}, {1000, 0}, {1300, -400}}, 11)};
    const openrider::sdk::SegmentNetwork net = matching::BuildNetwork(tracks, matching::MatchParams{});
    RequireConnected(net);
    int shared = 0;
    for (const openrider::sdk::NetworkSegment &segment : net.segments)
        shared += segment.Shared() ? 1 : 0;
    ATF_REQUIRE_EQ(shared, 1);
    // Junctions near both ends of the shared stretch.
    int near_start = 0, near_end = 0;
    for (const openrider::sdk::NetworkNode &node : net.nodes) {
        const auto [x, y] = At(node);
        near_start += std::hypot(x, y) < 40.0 ? 1 : 0;
        near_end += std::hypot(x - 1000.0, y) < 40.0 ? 1 : 0;
    }
    ATF_REQUIRE(near_start >= 1 && near_end >= 1);
}

ATF_TEST_CASE_WITHOUT_HEAD(close_parallel_legs_stay_apart);
ATF_TEST_CASE_BODY(close_parallel_legs_stay_apart)
{
    // A switchback: up a leg, a hairpin, back along a leg 25 m away. Both
    // tracks follow it; the two legs must stay two stretches of the line,
    // not be merged into one.
    const std::vector<std::pair<double, double>> zigzag = {{0, -300}, {0, 300}, {25, 320}, {25, -300}};
    const std::vector<openrider::sdk::OverviewTrack> tracks = {Track("a", zigzag, 3), Track("b", zigzag, 5)};
    const openrider::sdk::SegmentNetwork net = matching::BuildNetwork(tracks, matching::MatchParams{});
    RequireConnected(net);
    double shared_length = 0.0;
    for (const openrider::sdk::NetworkSegment &segment : net.segments)
        if (segment.Shared())
            shared_length += segment.length_m;
    ATF_REQUIRE(shared_length > 1000.0);  // both legs (~1250 m), not one
}

ATF_TEST_CASE_WITHOUT_HEAD(out_and_back_alone_is_one_segment_both_ways);
ATF_TEST_CASE_BODY(out_and_back_alone_is_one_segment_both_ways)
{
    const std::vector<openrider::sdk::OverviewTrack> tracks = {Track("a", {{0, 0}, {800, 0}, {0, 0}}, 9)};
    const openrider::sdk::SegmentNetwork net = matching::BuildNetwork(tracks, matching::MatchParams{});
    RequireConnected(net);
    double total = 0.0;
    for (const openrider::sdk::NetworkSegment &segment : net.segments)
        total += segment.length_m;
    ATF_REQUIRE(total < 1000.0);  // the way out and back is drawn once (~800 m)
    ATF_REQUIRE(net.routes.at(0).steps.size() >= 2);
}

ATF_TEST_CASE_WITHOUT_HEAD(plugin_abi);
ATF_TEST_CASE_BODY(plugin_abi)
{
    const or_plugin_info *info = openrider_plugin();
    ATF_REQUIRE(info && info->vtable && info->name);
    ATF_REQUIRE_EQ(info->api_version, static_cast<uint32_t>(OR_PLUGIN_API_VERSION));
    ATF_REQUIRE_EQ(info->kind, static_cast<uint32_t>(OR_PLUGIN_SEGMENTS));
    const auto *plugin = static_cast<const or_segments_vtable *>(openrider_plugin()->vtable);
    std::vector<double> values;
    for (size_t p = 0; p < plugin->param_count; ++p)
        values.push_back(plugin->params[p].default_value);
    const std::vector<openrider::sdk::OverviewTrack> tracks = {Track("a", {{0, 0}, {600, 0}}, 1), Track("b", {{0, 0}, {600, 0}}, 2)};
    const auto result = openrider::sdk::RunSegments(plugin, tracks, values, nullptr, nullptr);
    ATF_REQUIRE(result.Ok());
    ATF_REQUIRE_EQ(result.value.routes.size(), 2u);
    // Each route's steps cover its track, one after the other.
    for (size_t r = 0; r < tracks.size(); ++r) {
        const auto &steps = result.value.routes[r].steps;
        ATF_REQUIRE(!steps.empty());
        ATF_REQUIRE_EQ(steps.front().first, 0u);
        ATF_REQUIRE_EQ(steps.back().last, tracks[r].lat.size() - 1);
        for (size_t k = 1; k < steps.size(); ++k)
            ATF_REQUIRE_EQ(steps[k].first, steps[k - 1].last);
    }
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, shared_stretch_and_junctions);
    ATF_ADD_TEST_CASE(tcs, close_parallel_legs_stay_apart);
    ATF_ADD_TEST_CASE(tcs, out_and_back_alone_is_one_segment_both_ways);
    ATF_ADD_TEST_CASE(tcs, plugin_abi);
}
