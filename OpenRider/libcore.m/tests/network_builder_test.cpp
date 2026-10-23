// NetworkBuilder with fake input and segments plugins.

#include <atf-c++.hpp>

#include "core/network_builder.h"

#include <openrider/sdk/segment_exchange.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <thread>

namespace {

// Input plugin: every outing a straight track northwards from (45.9, 6.6),
// 500 m long, a point every 10 m; outing "indoor" has no position, outing
// "missing" cannot be read.
int FakePoints(void *, const char *id, or_point_cb cb, void *user, char *err, size_t errlen)
{
    if (std::string(id) == "missing") {
        std::snprintf(err, errlen, "no such outing");
        return 1;
    }
    const bool indoor = std::string(id) == "indoor";
    for (int i = 0; i <= 50; ++i) {
        or_point p{};
        p.t_s = i;
        p.lat = indoor ? std::nan("") : 45.9 + i * 10.0 / 111320.0;
        p.lon = indoor ? std::nan("") : 6.6;
        cb(user, &p);
    }
    return 0;
}
const or_input_vtable kInput = {sizeof(or_input_vtable), nullptr, nullptr, nullptr, FakePoints};

// The fake input as the only source.
const core::InputSet &Inputs()
{
    static core::InputSet *inputs = [] {
        auto *set = new core::InputSet;
        set->Add({&kInput, nullptr, "fake", "", false});
        return set;
    }();
    return *inputs;
}

// Segments plugin: one shared segment along the first track, followed by
// every track.
int FakeBuild(const or_segment_track *tracks, size_t count, const double *, or_segment_cancel_cb, void *,
              or_segment_network **out, char *, size_t)
{
    const std::vector<openrider::sdk::RecordedTrack> input = openrider::sdk::ImportTracks(tracks, count);
    openrider::sdk::SegmentNetwork network;
    const std::vector<or_point> &first = input[0].points;
    network.nodes = {{0, first.front().lon, first.front().lat}, {1, first.back().lon, first.back().lat}};
    openrider::sdk::NetworkSegment segment;
    segment.from = 0;
    segment.to = 1;
    for (const or_point &p : first) {
        segment.lon.push_back(p.lon);
        segment.lat.push_back(p.lat);
    }
    for (const openrider::sdk::RecordedTrack &track : input) {
        segment.route_ids.push_back(track.id);
        network.routes.push_back({track.id, {{0, true}}});
    }
    network.segments.push_back(segment);
    *out = openrider::sdk::ExportNetwork(network, input);
    return 0;
}

void FakeFree(or_segment_network *network) { openrider::sdk::FreeExportedNetwork(network); }

const or_filter_param kParams[] = {{"width", "Width", 8.0, 1.0, 50.0}};
const or_segments_vtable kFake = {sizeof(or_segments_vtable), "fake", "Fake", 1, kParams, FakeBuild, FakeFree};

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
    std::vector<core::TrackGroup> groups = {{"running", {"a", "b"}}, {"cycling", {"c", "d"}}};
    core::NetworkBuilder builder;
    builder.Start(groups, &kFake, {8.0}, &Inputs());
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
    builder.Start({{"running", {"a", "b"}}}, &kFake, {8.0}, &Inputs());
    builder.Start({{"hiking", {"c", "d"}}}, &kFake, {8.0}, &Inputs());
    const std::vector<core::SegmentGroup> result = Wait(builder);
    ATF_REQUIRE_EQ(result.size(), 1u);
    ATF_REQUIRE_EQ(result[0].sport, "hiking");
}

ATF_TEST_CASE_WITHOUT_HEAD(many_requests_coalesce);
ATF_TEST_CASE_BODY(many_requests_coalesce)
{
    core::NetworkBuilder builder;
    for (int width = 1; width <= 50; ++width)  // like a slider being dragged
        builder.Start({{"running", {"a", "b"}}}, &kFake, {double(width)}, &Inputs());
    ATF_REQUIRE_EQ(Wait(builder).size(), 1u);
    for (int i = 0; i < 1000 && builder.Building(); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    std::vector<core::SegmentGroup> again;
    ATF_REQUIRE(!builder.Take(again));  // only the latest request was published
}

// Outings without any position are left out, unreadable ones reported; a
// group left with fewer than two tracks has no network.
ATF_TEST_CASE_WITHOUT_HEAD(outings_without_positions_left_out);
ATF_TEST_CASE_BODY(outings_without_positions_left_out)
{
    core::NetworkBuilder builder;
    builder.Start({{"running", {"a", "indoor", "b"}}, {"rowing", {"c", "indoor"}}, {"hiking", {"d", "missing"}}},
                  &kFake, {8.0}, &Inputs());
    const std::vector<core::SegmentGroup> result = Wait(builder);
    ATF_REQUIRE_EQ(result.size(), 1u);
    ATF_REQUIRE_EQ(result[0].network.routes.size(), 2u);
    ATF_REQUIRE_EQ(result[0].network.routes[1].id, "b");
    ATF_REQUIRE(builder.Error().find("missing") != std::string::npos);
}

ATF_TEST_CASE_WITHOUT_HEAD(exchange_round_trip);
ATF_TEST_CASE_BODY(exchange_round_trip)
{
    const std::vector<openrider::sdk::RecordedTrack> tracks = {{"a", {}}, {"b", {}}};
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
    ATF_ADD_TEST_CASE(tcs, outings_without_positions_left_out);
    ATF_ADD_TEST_CASE(tcs, exchange_round_trip);
}
