/// @file
/// The segment network: the junctions (nodes) and segments the outings of
/// similar activities follow, each outing as a chain of segments. Segments
/// plugins (openrider/segments.h) identify it; these types are the
/// application's side of it.
#pragma once

#include <string>
#include <vector>

namespace openrider::sdk {

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
/// segments. A route turning around mid-segment cuts it there. Route
/// starts and ends join the node already within max(50 m, trace width)
/// (outings from home share one node); short single-route loops (GPS
/// wandering) are dropped; segments meeting at a node that is no junction,
/// turn-around or route end are joined. Nothing finer than the trace width
/// is resolved: entry points
/// within that width are one cut, and a segment shorter than it is
/// contracted into one node. Each segment lists its routes; each route is an ordered chain
/// of connected segments with its direction of travel on each.
struct SegmentNetwork {
    std::vector<NetworkNode> nodes;        ///< junctions
    std::vector<NetworkSegment> segments;  ///< shared and single-route segments
    std::vector<NetworkRoute> routes;      ///< one per input track
};

} // namespace openrider::sdk
