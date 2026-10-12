#include <atf-c++.hpp>

#include "core/segments.h"

#include <cmath>

namespace {

constexpr double kLat0 = 45.9, kLon0 = 6.6;
constexpr double kKx = 111320.0 * 0.69591;  // cos(45.9 deg)
constexpr double kKy = 110540.0;

// A track from metric points (x east, y north), one point every 10 m.
core::OverviewTrack track(const std::string &id, const std::vector<std::pair<double, double>> &corners,
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
std::vector<core::OverviewTrack> two_tracks()
{
    return {track("a", {{-300, 400}, {0, 0}, {1000, 0}, {1300, 400}}, 3.0, 7),
            track("b", {{-300, -400}, {0, 0}, {1000, 0}, {1300, -400}}, -3.0, 11)};
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
    core::thin_image(img, w, h);
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
    const auto r = core::find_segments(two_tracks(), core::SegmentParams{});
    ATF_REQUIRE_EQ(r.segments.size(), 1u);
    const core::Segment &s = r.segments[0];
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
    const auto r = core::find_segments(two_tracks(), core::SegmentParams{});
    ATF_REQUIRE_EQ(r.mappings.size(), 2u);
    for (const core::TrackMapping &m : r.mappings) {
        ATF_REQUIRE_EQ(m.pieces.size(), 3u);
        ATF_REQUIRE_EQ(m.pieces[0].segment, -1);
        ATF_REQUIRE_EQ(m.pieces[1].segment, 0);
        ATF_REQUIRE_EQ(m.pieces[2].segment, -1);
    }
}

ATF_TEST_CASE_WITHOUT_HEAD(single_track_has_no_common_segment);
ATF_TEST_CASE_BODY(single_track_has_no_common_segment)
{
    const auto r = core::find_segments({two_tracks()[0]}, core::SegmentParams{});
    ATF_REQUIRE(r.segments.empty());
    ATF_REQUIRE_EQ(r.mappings.size(), 1u);
    ATF_REQUIRE_EQ(r.mappings[0].pieces.size(), 1u);
    ATF_REQUIRE_EQ(r.mappings[0].pieces[0].segment, -1);
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, thinning_gives_one_pixel_line);
    ATF_ADD_TEST_CASE(tcs, shared_stretch_is_one_common_segment);
    ATF_ADD_TEST_CASE(tcs, tracks_mapped_own_common_own);
    ATF_ADD_TEST_CASE(tcs, single_track_has_no_common_segment);
}
