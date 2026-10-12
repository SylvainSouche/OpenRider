// Common route segments across outings: a heat map of the tracks is
// vectorized into a graph of segments; each track is then mapped onto the
// segments, and each common segment gets a mean track.
//
// Method (map inference from GPS traces, kernel-density style):
//  1. local metric projection; sparse grid of `cell_m` cells;
//  2. blurred heat map: each track adds 1 to the cells its line crosses and
//     0.5 to their 8 neighbours, once per cell (the higher value when both;
//     a track lingering somewhere does not make a corridor); tracks add up;
//  3. corridor = cells with density >= `min_density` reached by at least
//     `min_tracks` distinct tracks;
//  4. corridor thinned to a one-cell skeleton, traced junction to junction
//     into segments; short dead-end spurs pruned;
//  5. track points snapped to the nearest segment within tolerance: runs on
//     a segment are its common parts, the rest is its own;
//  6. mean track of a segment: offsets of the matched points from the
//     skeleton, averaged per track and then across tracks, along the
//     segment.
// Callers group tracks of similar activities before calling.
#pragma once

#include "core/overview.h"

#include <string>
#include <vector>

namespace core {

struct SegmentParams {
    double cell_m = 8.0;          // grid resolution
    double tolerance_m = 8.0;     // snapping tolerance when mapping tracks
    double min_density = 1.5;     // blurred heat needed for a corridor
    int min_tracks = 2;           // distinct tracks making a corridor
    double min_spur_m = 60.0;     // dead ends shorter than this are pruned
    double min_piece_m = 50.0;    // shorter runs on a segment are not counted
};

struct Segment {
    int id = 0;
    std::vector<double> lon, lat;  // mean track
    double length_m = 0.0;
    std::vector<std::string> track_ids;  // tracks following it (>= min_tracks)
};

struct TrackPiece {
    int segment = -1;           // -1: the track's own part
    size_t first = 0, last = 0; // point indices in the track, inclusive
};

struct TrackMapping {
    std::string id;
    std::vector<TrackPiece> pieces;  // in track order
};

struct SegmentResult {
    std::vector<Segment> segments;
    std::vector<TrackMapping> mappings;
};

SegmentResult find_segments(const std::vector<OverviewTrack> &tracks, const SegmentParams &p);

// Zhang-Suen thinning of a binary image (row-major, 1 = set), in place.
// Exposed for tests.
void thin_image(std::vector<unsigned char> &img, int width, int height);

} // namespace core
