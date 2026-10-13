#include <atf-c++.hpp>

#include "heatmap/heatmap.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr double kLat0 = 45.9, kLon0 = 6.6;
constexpr double kKx = 111320.0 * 0.69591;  // cos(45.9 deg)
constexpr double kKy = 110540.0;

// A track from metric points (x east, y north), one point every 10 m.
core::OverviewTrack Track(const std::string &id, const std::vector<std::pair<double, double>> &corners,
                          double offset, unsigned seed)
{
    core::OverviewTrack t;
    t.id = id;
    t.west = t.south = 1e9;
    t.east = t.north = -1e9;
    for (size_t c = 1; c < corners.size(); ++c) {
        const double x0 = corners[c - 1].first, y0 = corners[c - 1].second;
        const double x1 = corners[c].first, y1 = corners[c].second;
        const int n = static_cast<int>(std::hypot(x1 - x0, y1 - y0) / 10.0);
        for (int i = 0; i < n; ++i) {
            seed = seed * 1103515245u + 12345u;
            const double noise = ((seed >> 16) % 400) / 100.0 - 2.0;  // +/- 2 m
            const double x = x0 + (x1 - x0) * i / n, y = y0 + (y1 - y0) * i / n + offset + noise;
            t.lat.push_back(kLat0 + y / kKy);
            t.lon.push_back(kLon0 + x / kKx);
        }
    }
    for (size_t i = 0; i < t.lat.size(); ++i) {
        t.west = std::min(t.west, t.lon[i]);
        t.east = std::max(t.east, t.lon[i]);
        t.south = std::min(t.south, t.lat[i]);
        t.north = std::max(t.north, t.lat[i]);
    }
    return t;
}

// Two outings sharing y = 0 from x = 0 to x = 1000 m, arriving and leaving
// from different places, one 3 m north of the path and one 3 m south.
std::vector<core::OverviewTrack> TwoTracks()
{
    return {Track("a", {{-300, 400}, {0, 0}, {1000, 0}, {1300, 400}}, 3.0, 7),
            Track("b", {{-300, -400}, {0, 0}, {1000, 0}, {1300, -400}}, -3.0, 11)};
}

} // namespace

ATF_TEST_CASE_WITHOUT_HEAD(thinning_gives_one_pixel_line);
ATF_TEST_CASE_BODY(thinning_gives_one_pixel_line)
{
    const int w = 30, h = 9;
    std::vector<unsigned char> img(w * h, 0);
    for (int y = 2; y < 7; ++y)
        for (int x = 2; x < 28; ++x)
            img[y * w + x] = 1;
    heatmap::ThinImage(img, w, h);
    for (int x = 6; x < 24; ++x) {  // away from the ends, one pixel per column
        int n = 0;
        for (int y = 0; y < h; ++y)
            n += img[y * w + x];
        ATF_REQUIRE_EQ(n, 1);
    }
}

ATF_TEST_CASE_WITHOUT_HEAD(shared_stretch_is_one_common_segment);
ATF_TEST_CASE_BODY(shared_stretch_is_one_common_segment)
{
    const auto r = heatmap::FindSegments(TwoTracks(), heatmap::SegmentParams{});
    ATF_REQUIRE_EQ(r.segments.size(), 1u);
    const heatmap::Segment &s = r.segments[0];
    ATF_REQUIRE_EQ(s.track_ids.size(), 2u);
    ATF_REQUIRE(s.length_m > 850.0 && s.length_m < 1150.0);
    // The mean track runs between the two (y ~ 0), not along either.
    for (size_t i = 0; i < s.lat.size(); ++i) {
        const double x = (s.lon[i] - kLon0) * kKx, y = (s.lat[i] - kLat0) * kKy;
        if (x > 200 && x < 800)
            ATF_REQUIRE(std::fabs(y) < 2.0);
    }
}

ATF_TEST_CASE_WITHOUT_HEAD(tracks_mapped_own_common_own);
ATF_TEST_CASE_BODY(tracks_mapped_own_common_own)
{
    const auto r = heatmap::FindSegments(TwoTracks(), heatmap::SegmentParams{});
    ATF_REQUIRE_EQ(r.mappings.size(), 2u);
    for (const heatmap::TrackMapping &m : r.mappings) {
        ATF_REQUIRE_EQ(m.pieces.size(), 3u);
        ATF_REQUIRE_EQ(m.pieces[0].segment, -1);
        ATF_REQUIRE_EQ(m.pieces[1].segment, 0);
        ATF_REQUIRE_EQ(m.pieces[2].segment, -1);
    }
}

ATF_TEST_CASE_WITHOUT_HEAD(single_track_has_no_common_segment);
ATF_TEST_CASE_BODY(single_track_has_no_common_segment)
{
    const auto r = heatmap::FindSegments({TwoTracks()[0]}, heatmap::SegmentParams{});
    ATF_REQUIRE(r.segments.empty());
    ATF_REQUIRE_EQ(r.mappings.size(), 1u);
    ATF_REQUIRE_EQ(r.mappings[0].pieces.size(), 1u);
    ATF_REQUIRE_EQ(r.mappings[0].pieces[0].segment, -1);
}

namespace {

// Consecutive steps of every route share a node.
void RequireConnected(const core::SegmentNetwork &net)
{
    for (const core::NetworkRoute &r : net.routes)
        for (size_t i = 1; i < r.steps.size(); ++i) {
            const core::NetworkSegment &a = net.segments[static_cast<size_t>(r.steps[i - 1].segment)];
            const core::NetworkSegment &b = net.segments[static_cast<size_t>(r.steps[i].segment)];
            const int exit = r.steps[i - 1].forward ? a.to : a.from;
            const int entry = r.steps[i].forward ? b.from : b.to;
            ATF_REQUIRE_EQ(exit, entry);
        }
}

// The segment whose mean track passes nearest the metric point (x, y).
const core::NetworkSegment *SegmentAt(const core::SegmentNetwork &net, double x, double y)
{
    const core::NetworkSegment *best = nullptr;
    double bd = 1e18;
    // Distance to the polyline (a straight segment may keep only its ends).
    for (const core::NetworkSegment &s : net.segments)
        for (size_t i = 1; i < s.lat.size(); ++i) {
            const double ax = (s.lon[i - 1] - kLon0) * kKx, ay = (s.lat[i - 1] - kLat0) * kKy;
            const double bx = (s.lon[i] - kLon0) * kKx, by = (s.lat[i] - kLat0) * kKy;
            const double vx = bx - ax, vy = by - ay, l2 = vx * vx + vy * vy;
            double t = l2 > 0 ? ((x - ax) * vx + (y - ay) * vy) / l2 : 0.0;
            t = std::max(0.0, std::min(1.0, t));
            const double dx = ax + t * vx - x, dy = ay + t * vy - y;
            if (dx * dx + dy * dy < bd) {
                bd = dx * dx + dy * dy;
                best = &s;
            }
        }
    return best;
}

} // namespace

ATF_TEST_CASE_WITHOUT_HEAD(network_two_routes);
ATF_TEST_CASE_BODY(network_two_routes)
{
    const auto net = heatmap::BuildNetwork(TwoTracks(), heatmap::SegmentParams{});
    int shared = 0, own = 0;
    for (const core::NetworkSegment &s : net.segments)
        (s.Shared() ? shared : own) += 1;
    ATF_REQUIRE_EQ(shared, 1);
    ATF_REQUIRE_EQ(own, 4);              // two own parts per route
    ATF_REQUIRE_EQ(net.nodes.size(), 6u);  // 2 junctions + 4 route ends
    ATF_REQUIRE_EQ(net.routes.size(), 2u);
    for (const core::NetworkRoute &r : net.routes)
        ATF_REQUIRE_EQ(r.steps.size(), 3u);  // own, shared, own
    RequireConnected(net);
}

ATF_TEST_CASE_WITHOUT_HEAD(network_cut_where_a_route_joins);
ATF_TEST_CASE_BODY(network_cut_where_a_route_joins)
{
    // c joins the shared stretch halfway, from the north, and leaves with a.
    auto tracks = TwoTracks();
    tracks.push_back(Track("c", {{500, 500}, {500, 0}, {1000, 0}, {1300, 400}}, 0.0, 13));
    const auto net = heatmap::BuildNetwork(tracks, heatmap::SegmentParams{});
    const core::NetworkSegment *west = SegmentAt(net, 250, 0);
    const core::NetworkSegment *east = SegmentAt(net, 750, 0);
    ATF_REQUIRE(west && east && west != east);  // cut near x = 500
    ATF_REQUIRE_EQ(west->route_ids.size(), 2u);  // a, b
    ATF_REQUIRE_EQ(east->route_ids.size(), 3u);  // a, b, c
    const core::NetworkSegment *exit = SegmentAt(net, 1150, 200);
    ATF_REQUIRE(exit && exit->Shared());         // a and c leave together
    ATF_REQUIRE_EQ(exit->route_ids.size(), 2u);
    RequireConnected(net);
}

ATF_TEST_CASE_WITHOUT_HEAD(network_resolution_follows_trace_width);
ATF_TEST_CASE_BODY(network_resolution_follows_trace_width)
{
    // A wide trace merges the junctions: no segment is shorter than it, and
    // the routes stay chains of connected segments.
    auto tracks = TwoTracks();
    tracks.push_back(Track("c", {{500, 500}, {500, 0}, {1000, 0}, {1300, 400}}, 0.0, 13));
    tracks.push_back(Track("d", {{520, -500}, {520, 0}, {1000, 0}, {1300, -400}}, 0.0, 17));
    for (double width : {20.0, 50.0}) {
        heatmap::SegmentParams params;
        params.trace_width_m = width;
        const auto net = heatmap::BuildNetwork(tracks, params);
        for (const core::NetworkSegment &s : net.segments)
            ATF_REQUIRE(s.length_m >= width * 0.9);  // simplification may shorten a little
        RequireConnected(net);
        // c and d join 20 m apart: one cut, not two, at 50 m.
        if (width == 50.0) {
            const core::NetworkSegment *east = SegmentAt(net, 750, 0);
            ATF_REQUIRE(east && east->route_ids.size() == 4u);
        }
    }
}

ATF_TEST_CASE_WITHOUT_HEAD(turn_around_alone_is_one_segment_both_ways);
ATF_TEST_CASE_BODY(turn_around_alone_is_one_segment_both_ways)
{
    // a goes out to x = 1000 and comes back the same way; b shares the first
    // 400 m and leaves northwards. The stretch a does alone, out and back,
    // is one segment ending at the turn-around, travelled forward then
    // backward -- not a loop of its own track.
    std::vector<core::OverviewTrack> tracks = {
        Track("a", {{0, 0}, {1000, 0}, {0, 0}}, 0.0, 7),
        Track("b", {{0, 0}, {400, 0}, {400, 600}}, 0.0, 11)};
    const auto net = heatmap::BuildNetwork(tracks, heatmap::SegmentParams{});
    RequireConnected(net);
    const core::NetworkSegment *spur = SegmentAt(net, 750, 0);
    ATF_REQUIRE(spur);
    ATF_REQUIRE(spur->from != spur->to);  // not a loop
    ATF_REQUIRE_EQ(spur->route_ids.size(), 1u);
    const core::NetworkRoute &a = net.routes[0];
    size_t uses = 0;
    for (size_t i = 0; i < a.steps.size(); ++i)
        if (a.steps[i].segment == spur->id) {
            ++uses;
            if (i + 1 < a.steps.size() && a.steps[i + 1].segment == spur->id)
                ATF_REQUIRE(a.steps[i].forward != a.steps[i + 1].forward);  // out, then back
        }
    ATF_REQUIRE_EQ(uses, 2u);
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, turn_around_alone_is_one_segment_both_ways);
    ATF_ADD_TEST_CASE(tcs, network_resolution_follows_trace_width);
    ATF_ADD_TEST_CASE(tcs, thinning_gives_one_pixel_line);
    ATF_ADD_TEST_CASE(tcs, shared_stretch_is_one_common_segment);
    ATF_ADD_TEST_CASE(tcs, tracks_mapped_own_common_own);
    ATF_ADD_TEST_CASE(tcs, single_track_has_no_common_segment);
    ATF_ADD_TEST_CASE(tcs, network_two_routes);
    ATF_ADD_TEST_CASE(tcs, network_cut_where_a_route_joins);
}
