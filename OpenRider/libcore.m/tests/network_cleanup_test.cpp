// Network clean-up on hand-made networks.

#include <atf-c++.hpp>

#include "core/network_cleanup.h"

namespace {

// A straight segment between two nodes, a little over `length_m` long.
core::NetworkSegment Segment(int id, int from, int to, double lon0, double lon1, std::vector<std::string> routes)
{
    core::NetworkSegment segment;
    segment.id = id;
    segment.from = from;
    segment.to = to;
    segment.lon = {lon0, lon1};
    segment.lat = {45.9, 45.9};
    segment.length_m = std::abs(lon1 - lon0) * 77000.0;
    segment.route_ids = std::move(routes);
    return segment;
}

} // namespace

ATF_TEST_CASE_WITHOUT_HEAD(same_routes_without_branching_are_one_segment);
ATF_TEST_CASE_BODY(same_routes_without_branching_are_one_segment)
{
    // n0 -A- n1 -B- n2 -C- n3. Both routes use A and B; r2 turns back at n1
    // (out along A and back) and covers B on another leg: the counts are
    // the same on A and B, so they join although r2 turns at n1.
    core::SegmentNetwork net;
    net.nodes = {{0, 6.60, 45.9}, {1, 6.61, 45.9}, {2, 6.62, 45.9}, {3, 6.63, 45.9}};
    net.segments = {Segment(0, 0, 1, 6.60, 6.61, {"r1", "r2"}), Segment(1, 1, 2, 6.61, 6.62, {"r1", "r2"}),
                    Segment(2, 2, 3, 6.62, 6.63, {"r2"})};
    net.routes = {{"r1", {{0, true}, {1, true}}},
                  {"r2", {{0, true}, {0, false}, {2, false}, {1, false}, {1, true}, {2, true}}}};
    core::MergeThroughNodes(net, 10.0);  // turns of 770 m are no jitter
    ATF_REQUIRE_EQ(net.segments.size(), 2u);  // A+B, and C
    // r1 passes straight through: a single step on the merged segment.
    ATF_REQUIRE_EQ(net.routes[0].steps.size(), 1u);
}

ATF_TEST_CASE_WITHOUT_HEAD(different_routes_keep_the_cut);
ATF_TEST_CASE_BODY(different_routes_keep_the_cut)
{
    // r2 turns back at n1 and never uses B: A and B carry different routes.
    core::SegmentNetwork net;
    net.nodes = {{0, 6.60, 45.9}, {1, 6.61, 45.9}, {2, 6.62, 45.9}};
    net.segments = {Segment(0, 0, 1, 6.60, 6.61, {"r1", "r2"}), Segment(1, 1, 2, 6.61, 6.62, {"r1"})};
    net.routes = {{"r1", {{0, true}, {1, true}}}, {"r2", {{0, true}, {0, false}}}};
    core::MergeThroughNodes(net, 10.0);
    ATF_REQUIRE_EQ(net.segments.size(), 2u);
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, same_routes_without_branching_are_one_segment);
    ATF_ADD_TEST_CASE(tcs, different_routes_keep_the_cut);
}
