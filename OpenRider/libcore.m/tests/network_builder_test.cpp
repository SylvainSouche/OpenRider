#include <atf-c++.hpp>

#include "core/network_builder.h"

#include <chrono>
#include <thread>

namespace {

// A straight track northwards from (45.9, 6.6), `length_m` long, points every 10 m.
core::OverviewTrack Straight(const std::string &id, double length_m)
{
    core::OverviewTrack track;
    track.id = id;
    for (double d = 0.0; d <= length_m; d += 10.0) {
        track.lat.push_back(45.9 + d / 111320.0);
        track.lon.push_back(6.6);
    }
    track.west = track.east = 6.6;
    track.south = track.lat.front();
    track.north = track.lat.back();
    return track;
}

} // namespace

ATF_TEST_CASE_WITHOUT_HEAD(builds_one_network_per_group);
ATF_TEST_CASE_BODY(builds_one_network_per_group)
{
    std::vector<core::TrackGroup> groups = {{"running", {Straight("a", 500), Straight("b", 500)}},
                                            {"cycling", {Straight("c", 500), Straight("d", 500)}}};
    core::NetworkBuilder builder;
    builder.Start(groups, core::SegmentParams{});
    std::vector<core::SegmentGroup> result;
    for (int i = 0; i < 1000 && !builder.Take(result); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    ATF_REQUIRE_EQ(result.size(), 2u);
    ATF_REQUIRE_EQ(result[0].sport, "running");
    bool shared = false;
    for (const core::NetworkSegment &segment : result[0].network.segments)
        shared = shared || segment.Shared();
    ATF_REQUIRE(shared);
    ATF_REQUIRE(!builder.Building());
    ATF_REQUIRE(!builder.Take(result));  // taken once
}

ATF_TEST_CASE_WITHOUT_HEAD(restart_replaces_the_build);
ATF_TEST_CASE_BODY(restart_replaces_the_build)
{
    core::NetworkBuilder builder;
    builder.Start({{"running", {Straight("a", 500), Straight("b", 500)}}}, core::SegmentParams{});
    builder.Start({{"hiking", {Straight("c", 500), Straight("d", 500)}}}, core::SegmentParams{});
    std::vector<core::SegmentGroup> result;
    for (int i = 0; i < 1000 && !builder.Take(result); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    ATF_REQUIRE_EQ(result.size(), 1u);
    ATF_REQUIRE_EQ(result[0].sport, "hiking");
}

ATF_TEST_CASE_WITHOUT_HEAD(many_requests_coalesce);
ATF_TEST_CASE_BODY(many_requests_coalesce)
{
    core::NetworkBuilder builder;
    core::SegmentParams params;
    for (int width = 1; width <= 50; ++width) {  // like a slider being dragged
        params.trace_width_m = width;
        builder.Start({{"running", {Straight("a", 500), Straight("b", 500)}}}, params);
    }
    std::vector<core::SegmentGroup> result;
    for (int i = 0; i < 2000 && !builder.Take(result); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    ATF_REQUIRE_EQ(result.size(), 1u);
    for (int i = 0; i < 1000 && builder.Building(); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    ATF_REQUIRE(!builder.Building());
    ATF_REQUIRE(!builder.Take(result));  // only the latest request was published
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, builds_one_network_per_group);
    ATF_ADD_TEST_CASE(tcs, restart_replaces_the_build);
    ATF_ADD_TEST_CASE(tcs, many_requests_coalesce);
}
