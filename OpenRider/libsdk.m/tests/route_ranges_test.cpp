// Point ranges of route steps on their tracks.

#include <atf-c++.hpp>

#include <openrider/sdk/route_ranges.h>

#include <cmath>

namespace {

constexpr double kLat0 = 45.9, kLon0 = 6.6;
constexpr double kKx = 111320.0 * 0.69591, kKy = 110540.0;

double Lon(double x) { return kLon0 + x / kKx; }
double Lat(double y) { return kLat0 + y / kKy; }

// A track every 10 m through metric corners.
openrider::sdk::OverviewTrack Track(const std::string &id, const std::vector<std::pair<double, double>> &corners)
{
    openrider::sdk::OverviewTrack track;
    track.id = id;
    for (size_t c = 1; c < corners.size(); ++c) {
        const double x0 = corners[c - 1].first, y0 = corners[c - 1].second;
        const double x1 = corners[c].first, y1 = corners[c].second;
        const int n = std::max(1, static_cast<int>(std::hypot(x1 - x0, y1 - y0) / 10.0));
        for (int i = 0; i < n; ++i) {
            track.lon.push_back(Lon(x0 + (x1 - x0) * i / n));
            track.lat.push_back(Lat(y0 + (y1 - y0) * i / n));
        }
    }
    track.lon.push_back(Lon(corners.back().first));
    track.lat.push_back(Lat(corners.back().second));
    return track;
}

// A straight segment between two nodes.
openrider::sdk::NetworkSegment Segment(int id, int from, int to, std::pair<double, double> a, std::pair<double, double> b)
{
    openrider::sdk::NetworkSegment segment;
    segment.id = id;
    segment.from = from;
    segment.to = to;
    segment.lon = {Lon(a.first), Lon(b.first)};
    segment.lat = {Lat(a.second), Lat(b.second)};
    return segment;
}

// Index of the track's point nearest (x, y).
size_t Nearest(const openrider::sdk::OverviewTrack &track, double x, double y)
{
    size_t best = 0;
    for (size_t i = 0; i < track.lat.size(); ++i)
        if (std::hypot((track.lon[i] - Lon(x)) * kKx, (track.lat[i] - Lat(y)) * kKy) <
            std::hypot((track.lon[best] - Lon(x)) * kKx, (track.lat[best] - Lat(y)) * kKy))
            best = i;
    return best;
}

} // namespace

ATF_TEST_CASE_WITHOUT_HEAD(corner_cuts_the_track_at_the_node);
ATF_TEST_CASE_BODY(corner_cuts_the_track_at_the_node)
{
    // East 500 m, then north 300 m: two segments meeting at node 1.
    const openrider::sdk::OverviewTrack track = Track("a", {{0, 2}, {500, 2}, {500, 300}});
    openrider::sdk::SegmentNetwork network;
    network.nodes = {{0, Lon(0), Lat(0)}, {1, Lon(500), Lat(0)}, {2, Lon(500), Lat(300)}};
    network.segments = {Segment(0, 0, 1, {0, 0}, {500, 0}), Segment(1, 1, 2, {500, 0}, {500, 300})};
    network.routes = {{"a", {{0, true}, {1, true}}}};
    openrider::sdk::AssignStepRanges(network, {track});
    const auto &steps = network.routes[0].steps;
    ATF_REQUIRE_EQ(steps[0].first, 0u);
    ATF_REQUIRE_EQ(steps[1].last, track.lat.size() - 1);
    ATF_REQUIRE_EQ(steps[0].last, steps[1].first);
    const size_t corner = Nearest(track, 500, 2);
    ATF_REQUIRE(steps[1].first + 1 >= corner && steps[1].first <= corner + 1);
}

ATF_TEST_CASE_WITHOUT_HEAD(out_and_back_cut_at_the_turn);
ATF_TEST_CASE_BODY(out_and_back_cut_at_the_turn)
{
    // Out 400 m east and back on the same segment, turning at node 1: the
    // cut is the turn, though every point is as close to either step.
    const openrider::sdk::OverviewTrack track = Track("a", {{0, 0}, {400, 3}, {0, 0}});
    openrider::sdk::SegmentNetwork network;
    network.nodes = {{0, Lon(0), Lat(0)}, {1, Lon(400), Lat(0)}};
    network.segments = {Segment(0, 0, 1, {0, 0}, {400, 0})};
    network.routes = {{"a", {{0, true}, {0, false}}}};
    openrider::sdk::AssignStepRanges(network, {track});
    const auto &steps = network.routes[0].steps;
    const size_t turn = Nearest(track, 400, 3);
    ATF_REQUIRE(steps[0].last + 1 >= turn && steps[0].last <= turn + 1);
    ATF_REQUIRE_EQ(steps[0].last, steps[1].first);
    ATF_REQUIRE_EQ(steps[1].last, track.lat.size() - 1);
}

ATF_TEST_CASE_WITHOUT_HEAD(fewer_points_than_steps);
ATF_TEST_CASE_BODY(fewer_points_than_steps)
{
    openrider::sdk::OverviewTrack track = Track("a", {{0, 0}, {5, 0}});  // two points
    openrider::sdk::SegmentNetwork network;
    network.nodes = {{0, Lon(0), Lat(0)}, {1, Lon(5), Lat(0)}};
    network.segments = {Segment(0, 0, 1, {0, 0}, {5, 0})};
    network.routes = {{"a", {{0, true}, {0, false}, {0, true}}}};
    openrider::sdk::AssignStepRanges(network, {track});
    const auto &steps = network.routes[0].steps;
    ATF_REQUIRE_EQ(steps[0].first, 0u);
    ATF_REQUIRE_EQ(steps[2].last, 1u);
    for (size_t k = 1; k < steps.size(); ++k)
        ATF_REQUIRE_EQ(steps[k].first, steps[k - 1].last);
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, corner_cuts_the_track_at_the_node);
    ATF_ADD_TEST_CASE(tcs, out_and_back_cut_at_the_turn);
    ATF_ADD_TEST_CASE(tcs, fewer_points_than_steps);
}
