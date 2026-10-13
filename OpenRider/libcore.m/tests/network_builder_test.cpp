// NetworkBuilder with a fake segments plugin: one shared segment along the
// first track, followed by every track.

#include <atf-c++.hpp>

#include "core/network_builder.h"

#include <openrider/sdk/segment_exchange.h>

#include <chrono>
#include <thread>

namespace {

// A straight track northwards from (45.9, 6.6), `length_m` long, points every 10 m.
openrider::sdk::OverviewTrack Straight(const std::string &id, double length_m)
{
    openrider::sdk::OverviewTrack track;
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

int FakeBuild(const or_segment_track *tracks, size_t count, const double *, or_segment_cancel_cb, void *,
              or_segment_network **out, char *, size_t)
{
    const std::vector<openrider::sdk::OverviewTrack> input = openrider::sdk::ImportTracks(tracks, count);
    openrider::sdk::SegmentNetwork network;
    network.nodes = {{0, input[0].lon.front(), input[0].lat.front()}, {1, input[0].lon.back(), input[0].lat.back()}};
    openrider::sdk::NetworkSegment segment;
    segment.from = 0;
    segment.to = 1;
    segment.lon = input[0].lon;
    segment.lat = input[0].lat;
    for (const openrider::sdk::OverviewTrack &track : input) {
        segment.route_ids.push_back(track.id);
        network.routes.push_back({track.id, {{0, true}}});
    }
    network.segments.push_back(segment);
    *out = openrider::sdk::ExportNetwork(network, input);
    return 0;
}

void FakeFree(or_segment_network *network) { openrider::sdk::FreeExportedNetwork(network); }

const or_filter_param kParams[] = {{"width", "Width", 8.0, 1.0, 50.0}};
const or_segments_vtable kFake = {"fake", "Fake", 1, kParams, FakeBuild, FakeFree};

std::vector<core::SegmentGroup> Wait(core::NetworkBuilder &builder)
{
    std::vector<core::SegmentGroup> result;
    for (int i = 0; i < 2000 && !builder.Take(result); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    return result;
}

} // namespace

ATF_TEST_CASE_WITHOUT_HEAD(builds_one_network_per_group);
ATF_TEST_CASE_BODY(builds_one_network_per_group)
{
    std::vector<core::TrackGroup> groups = {{"running", {Straight("a", 500), Straight("b", 500)}},
                                            {"cycling", {Straight("c", 500), Straight("d", 500)}}};
    core::NetworkBuilder builder;
    builder.Start(groups, &kFake, {8.0});
    const std::vector<core::SegmentGroup> result = Wait(builder);
    ATF_REQUIRE_EQ(result.size(), 2u);
    ATF_REQUIRE_EQ(result[0].sport, "running");
    ATF_REQUIRE(result[0].network.segments.at(0).Shared());
    ATF_REQUIRE_EQ(result[1].network.routes.at(1).id, "d");
    for (int i = 0; i < 1000 && builder.Building(); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    ATF_REQUIRE(!builder.Building());
    std::vector<core::SegmentGroup> again;
    ATF_REQUIRE(!builder.Take(again));  // taken once
}

ATF_TEST_CASE_WITHOUT_HEAD(restart_replaces_the_build);
ATF_TEST_CASE_BODY(restart_replaces_the_build)
{
    core::NetworkBuilder builder;
    builder.Start({{"running", {Straight("a", 500), Straight("b", 500)}}}, &kFake, {8.0});
    builder.Start({{"hiking", {Straight("c", 500), Straight("d", 500)}}}, &kFake, {8.0});
    const std::vector<core::SegmentGroup> result = Wait(builder);
    ATF_REQUIRE_EQ(result.size(), 1u);
    ATF_REQUIRE_EQ(result[0].sport, "hiking");
}

ATF_TEST_CASE_WITHOUT_HEAD(many_requests_coalesce);
ATF_TEST_CASE_BODY(many_requests_coalesce)
{
    core::NetworkBuilder builder;
    for (int width = 1; width <= 50; ++width)  // like a slider being dragged
        builder.Start({{"running", {Straight("a", 500), Straight("b", 500)}}}, &kFake, {double(width)});
    ATF_REQUIRE_EQ(Wait(builder).size(), 1u);
    for (int i = 0; i < 1000 && builder.Building(); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    std::vector<core::SegmentGroup> again;
    ATF_REQUIRE(!builder.Take(again));  // only the latest request was published
}

ATF_TEST_CASE_WITHOUT_HEAD(exchange_round_trip);
ATF_TEST_CASE_BODY(exchange_round_trip)
{
    const std::vector<openrider::sdk::OverviewTrack> tracks = {Straight("a", 100), Straight("b", 100)};
    openrider::sdk::SegmentNetwork network;
    network.nodes = {{0, 6.6, 45.9}, {1, 6.6, 45.901}};
    openrider::sdk::NetworkSegment segment;
    segment.from = 0;
    segment.to = 1;
    segment.lon = {6.6, 6.6};
    segment.lat = {45.9, 45.901};
    segment.length_m = 111.0;
    segment.route_ids = {"b"};
    network.segments.push_back(segment);
    network.routes = {{"a", {}}, {"b", {{0, false}}}};
    or_segment_network *exported = openrider::sdk::ExportNetwork(network, tracks);
    const openrider::sdk::SegmentNetwork back = openrider::sdk::ImportNetwork(*exported, tracks);
    openrider::sdk::FreeExportedNetwork(exported);
    ATF_REQUIRE_EQ(back.segments.size(), 1u);
    ATF_REQUIRE_EQ(back.segments[0].route_ids.at(0), "b");
    ATF_REQUIRE_EQ(back.segments[0].length_m, 111.0);
    ATF_REQUIRE_EQ(back.routes.size(), 2u);
    ATF_REQUIRE(back.routes[0].steps.empty());
    ATF_REQUIRE(!back.routes[1].steps.at(0).forward);
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, builds_one_network_per_group);
    ATF_ADD_TEST_CASE(tcs, restart_replaces_the_build);
    ATF_ADD_TEST_CASE(tcs, many_requests_coalesce);
    ATF_ADD_TEST_CASE(tcs, exchange_round_trip);
}
