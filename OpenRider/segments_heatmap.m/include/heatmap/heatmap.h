/// @file
/// Heat-map segment identification: the segments_heatmap plugin's
/// algorithm.
///
/// Common route segments across outings: a heat map of the tracks is
/// vectorized into a graph of segments; each track is then mapped onto the
/// segments, and each common segment gets a mean track.
///
/// Method (map inference from GPS traces, kernel-density style):
///  1. local metric projection; sparse grid of `cell_m` cells;
///  2. blurred heat map: each track is drawn as a line `trace_width_m` wide,
///     adding 1 to the cells it covers and 0.5 to a one-cell rim around them,
///     once per cell (the higher value when both; a track lingering somewhere
///     does not make a corridor); tracks add up. Wider traces merge routes
///     that run close together. Each track counts once per pass: its
///     turn-arounds (out-and-backs) split it into passes, so a stretch done
///     out and back forms a corridor even when no other outing follows it;
///  3. corridor = cells with density >= `min_density` reached by at least
///     `min_tracks` distinct tracks;
///  4. corridor thinned to a one-cell skeleton, traced junction to junction
///     into segments; short dead-end spurs pruned;
///  5. track points snapped to the nearest segment within tolerance (25 m at
///     least: GPS drift on a shared path): runs on
///     a segment are its common parts, the rest is its own;
///  6. mean track of a segment: offsets of the matched points from the
///     skeleton, averaged per track and then across tracks, along the
///     segment.
/// Callers group tracks of similar activities before calling.
#pragma once

#include "core/overview.h"
#include "core/segments.h"

#include <functional>
#include <string>
#include <vector>

namespace heatmap {

using core::NetworkNode;
using core::NetworkRoute;
using core::NetworkSegment;
using core::OverviewTrack;
using core::RouteStep;
using core::SegmentNetwork;

/// Parameters of segment inference.
struct SegmentParams {
    double cell_m = 8.0;          ///< grid resolution
    double tolerance_m = 8.0;     ///< snapping tolerance when mapping tracks (at least half the trace width)
    double trace_width_m = 8.0;   ///< width of the line each track is drawn with in the heat map
    double min_density = 1.5;     ///< blurred heat needed for a corridor
    int min_tracks = 2;           ///< distinct tracks making a corridor
    double min_spur_m = 60.0;     ///< dead ends shorter than this are pruned
    double min_piece_m = 50.0;    ///< shorter runs on a segment are not counted
    double max_excursion_m = 150.0;  ///< leaving a segment and rejoining it within
                                     ///< this distance is not leaving it
};

/// A common segment.
struct Segment {
    int id = 0;                           ///< index in SegmentResult::segments
    std::vector<double> lon;              ///< mean track, longitudes
    std::vector<double> lat;              ///< mean track, latitudes
    double length_m = 0.0;                ///< length of the mean track
    std::vector<std::string> track_ids;  ///< tracks following it (>= min_tracks)
};

/// A run of a track: on a segment, or on its own.
struct TrackPiece {
    int segment = -1;  ///< segment id; -1: the track's own part
    size_t first = 0;  ///< first point index in the track
    size_t last = 0;   ///< last point index in the track, inclusive
};

/// A track as a sequence of pieces.
struct TrackMapping {
    std::string id;                  ///< track (outing) id
    std::vector<TrackPiece> pieces;  ///< in track order
};

/// The outcome of FindSegments().
struct SegmentResult {
    std::vector<Segment> segments;       ///< common segments
    std::vector<TrackMapping> mappings;  ///< one per input track
};

/// Infers the common segments of `tracks` (see the file description).
SegmentResult FindSegments(const std::vector<OverviewTrack> &tracks, const SegmentParams &params);

/// Builds the segment network of `tracks`.
///
/// `cancelled`, when given, is polled between the costly steps; once it
/// returns true the result is incomplete and must be discarded.
SegmentNetwork BuildNetwork(const std::vector<OverviewTrack> &tracks, const SegmentParams &params,
                            const std::function<bool()> &cancelled = {});

/// Zhang-Suen thinning of a binary image (row-major, 1 = set), in place.
/// Exposed for tests.
void ThinImage(std::vector<unsigned char> &image, int width, int height);

} // namespace heatmap
