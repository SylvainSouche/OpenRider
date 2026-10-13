#include "matching/output.h"

#include "matching/costs.h"
#include "matching/track_geometry.h"

namespace matching {

using core::Length;
using core::XY;

core::SegmentNetwork ToSegmentNetwork(const Network &network, TrackChains chains,
                                      const std::vector<core::OverviewTrack> &tracks,
                                      const core::LocalProjection &projection)
{
    core::SegmentNetwork result;
    for (size_t n = 0; n < network.nodes.size(); ++n) {
        core::NetworkNode node;
        node.id = static_cast<int>(n);
        projection.Inverse(network.nodes[n], node.lon, node.lat);
        result.nodes.push_back(node);
    }
    for (size_t e = 0; e < network.edges.size(); ++e) {
        const Edge &edge = network.edges[e];
        core::NetworkSegment segment;
        segment.id = static_cast<int>(e);
        segment.from = edge.from;
        segment.to = edge.to;
        std::vector<XY> line = Smoothed(edge.stations, kSmoothStations);
        line.front() = network.nodes[static_cast<size_t>(edge.from)];  // the line meets its nodes
        line.back() = network.nodes[static_cast<size_t>(edge.to)];
        segment.length_m = Length(line);
        for (const XY &q : line) {
            double lon, lat;
            projection.Inverse(q, lon, lat);
            segment.lon.push_back(lon);
            segment.lat.push_back(lat);
        }
        for (int t : chains.users[e])
            segment.route_ids.push_back(tracks[static_cast<size_t>(t)].id);
        result.segments.push_back(std::move(segment));
    }
    result.routes = std::move(chains.routes);
    return result;
}

} // namespace matching
