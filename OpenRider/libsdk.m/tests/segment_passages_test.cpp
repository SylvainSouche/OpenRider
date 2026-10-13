// Passages of an outing along a segment.

#include <atf-c++.hpp>

#include <openrider/sdk/segment_passages.h>

#include <cmath>

namespace {

constexpr double kLat0 = 45.9, kLon0 = 6.6;
constexpr double kKx = 111320.0 * 0.69591, kKy = 110540.0;

// A segment along y = 0 from x = 0 to 1000 m.
openrider::sdk::NetworkSegment Segment()
{
    openrider::sdk::NetworkSegment segment;
    segment.lon = {kLon0, kLon0 + 500.0 / kKx, kLon0 + 1000.0 / kKx};
    segment.lat = {kLat0, kLat0, kLat0};
    segment.length_m = 1000.0;
    return segment;
}

// Points every 10 m through metric corners, 1 s apart.
std::vector<or_point> Points(const std::vector<std::pair<double, double>> &corners)
{
    std::vector<or_point> points;
    for (size_t c = 1; c < corners.size(); ++c) {
        const double x0 = corners[c - 1].first, y0 = corners[c - 1].second;
        const double x1 = corners[c].first, y1 = corners[c].second;
        const int n = std::max(1, static_cast<int>(std::hypot(x1 - x0, y1 - y0) / 10.0));
        for (int i = 0; i < n; ++i) {
            or_point point{};
            point.t_s = static_cast<double>(points.size());
            point.lat = kLat0 + (y0 + (y1 - y0) * i / n) / kKy;
            point.lon = kLon0 + (x0 + (x1 - x0) * i / n) / kKx;
            points.push_back(point);
        }
    }
    return points;
}

} // namespace

ATF_TEST_CASE_WITHOUT_HEAD(one_passage_through);
ATF_TEST_CASE_BODY(one_passage_through)
{
    // Arrives from the north-west, runs the segment, leaves north-east.
    const auto passages = openrider::sdk::FindPassages(Points({{-300, 300}, {0, 3}, {1000, 3}, {1300, 300}}), Segment(), 25.0, 0.8);
    ATF_REQUIRE_EQ(passages.size(), 1u);
    ATF_REQUIRE(passages[0].forward);
    ATF_REQUIRE(passages[0].along_m.front() < 50.0);
    ATF_REQUIRE(passages[0].along_m.back() > 950.0);
}

ATF_TEST_CASE_WITHOUT_HEAD(out_and_back_is_two_passages);
ATF_TEST_CASE_BODY(out_and_back_is_two_passages)
{
    const auto passages = openrider::sdk::FindPassages(Points({{-200, 0}, {1200, 0}, {-200, 0}}), Segment(), 25.0, 0.8);
    ATF_REQUIRE_EQ(passages.size(), 2u);
    ATF_REQUIRE(passages[0].forward);
    ATF_REQUIRE(!passages[1].forward);
}

ATF_TEST_CASE_WITHOUT_HEAD(partial_or_far_is_no_passage);
ATF_TEST_CASE_BODY(partial_or_far_is_no_passage)
{
    ATF_REQUIRE(openrider::sdk::FindPassages(Points({{0, 0}, {300, 0}}), Segment(), 25.0, 0.8).empty());     // 30% only
    ATF_REQUIRE(openrider::sdk::FindPassages(Points({{0, 100}, {1000, 100}}), Segment(), 25.0, 0.8).empty());  // 100 m away
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, one_passage_through);
    ATF_ADD_TEST_CASE(tcs, out_and_back_is_two_passages);
    ATF_ADD_TEST_CASE(tcs, partial_or_far_is_no_passage);
}
