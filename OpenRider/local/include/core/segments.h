/// @file
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
///     that run close together;
///  3. corridor = cells with density >= `min_density` reached by at least
///     `min_tracks` distinct tracks;
///  4. corridor thinned to a one-cell skeleton, traced junction to junction
///     into segments; short dead-end spurs pruned;
///  5. track points snapped to the nearest segment within tolerance: runs on
///     a segment are its common parts, the rest is its own;
///  6. mean track of a segment: offsets of the matched points from the
///     skeleton, averaged per track and then across tracks, along the
///     segment.
/// Callers group tracks of similar activities before calling.
#pragma once

#include "core/overview.h"

#include <string>
#include <vector>

namespace core {

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

/// A junction of the segment network.
struct NetworkNode {
    int id = 0;        ///< index in SegmentNetwork::nodes
    double lon = 0.0;  ///< position, longitude
    double lat = 0.0;  ///< position, latitude
};

/// A segment of the network, between two nodes.
struct NetworkSegment {
    int id = 0;                          ///< index in SegmentNetwork::segments
    int from = -1;                       ///< start node id, in the segment's own direction
    int to = -1;                         ///< end node id
    std::vector<double> lon;             ///< mean track (shared) or the route's own track, longitudes
    std::vector<double> lat;             ///< latitudes
    double length_m = 0.0;               ///< length of the track
    std::vector<std::string> route_ids;  ///< routes passing along it
    /// Followed by at least two routes.
    bool Shared() const { return route_ids.size() >= 2; }
};

/// One segment of a route.
struct RouteStep {
    int segment = -1;     ///< segment id
    bool forward = true;  ///< travelled from `from` to `to`
};

/// A route as a chain of segments.
struct NetworkRoute {
    std::string id;                ///< track (outing) id
    std::vector<RouteStep> steps;  ///< consecutive steps share a node
};

/// The segment network: every route entirely covered by segments joined at
/// nodes. Shared segments are cut where routes enter or leave them (entry
/// points close together merged), and turn-arounds, so that each route
/// covers whole segments; the parts a route does alone are single-route
/// segments. Each segment lists its routes; each route is an ordered chain
/// of connected segments with its direction of travel on each.
struct SegmentNetwork {
    std::vector<NetworkNode> nodes;        ///< junctions
    std::vector<NetworkSegment> segments;  ///< shared and single-route segments
    std::vector<NetworkRoute> routes;      ///< one per input track
};

/// Builds the segment network of `tracks`.
SegmentNetwork BuildNetwork(const std::vector<OverviewTrack> &tracks, const SegmentParams &params);

/// Zhang-Suen thinning of a binary image (row-major, 1 = set), in place.
/// Exposed for tests.
void ThinImage(std::vector<unsigned char> &image, int width, int height);

} // namespace core
