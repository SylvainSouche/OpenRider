/// @file
/// The network under construction while tracks are matched into it.
#pragma once

#include <openrider/sdk/plane_geometry.h>
#include <openrider/sdk/segments.h>

#include <cmath>
#include <cstdint>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

namespace matching {

/// A segment under construction: its stations from `from` to `to`, the sum
/// and number of the track points matched to each (their mean is the
/// station's position), and the tracks following it.
struct Edge {
    int from = -1;                      ///< start node
    int to = -1;                        ///< end node
    std::vector<openrider::sdk::XY> stations;     ///< points along it, about station_m apart
    std::vector<openrider::sdk::XY> sums;         ///< per station: sum of the matched points
    std::vector<double> weights;        ///< per station: number of matched points
    std::set<int> routes;               ///< tracks matched to it (input indices)
};

/// The network under construction, in the local plane, with a grid index
/// of its stations for nearest-station queries.
class Network {
public:
    /// @param cell size of the index grid (m), about the matching distance
    explicit Network(double cell) : cell_(cell) {}

    std::vector<openrider::sdk::XY> nodes;  ///< junction and end positions
    std::vector<Edge> edges;      ///< segments under construction

    /// Rebuilds the station index (after stations moved or were added).
    void Reindex();

    /// Calls `visit(edge, station)` for every station within `radius` of `q`.
    template <class Visit>
    void Near(const openrider::sdk::XY &q, double radius, Visit visit) const
    {
        const int64_t cx = static_cast<int64_t>(std::floor(q.x / cell_));
        const int64_t cy = static_cast<int64_t>(std::floor(q.y / cell_));
        const int64_t reach = static_cast<int64_t>(std::ceil(radius / cell_));
        for (int64_t dy = -reach; dy <= reach; ++dy)
            for (int64_t dx = -reach; dx <= reach; ++dx) {
                const auto found = index_.find(Pack(cx + dx, cy + dy));
                if (found == index_.end())
                    continue;
                for (auto [e, s] : found->second)
                    if (openrider::sdk::Dist(edges[static_cast<size_t>(e)].stations[static_cast<size_t>(s)], q) <= radius)
                        visit(e, s);
            }
    }

    /// Direction of edge `e` at station `s` (not normalised).
    openrider::sdk::XY Tangent(int e, int s) const;

    /// Adds a node at `q`.
    /// @return its index
    int AddNode(const openrider::sdk::XY &q);

    /// An existing node within `radius` of `q`, or a new one there.
    int NodeAt(const openrider::sdk::XY &q, double radius);

    /// Cuts edge `e` at station `s` and returns the node there: its end
    /// node when `s` is within kEndStations of an end (no stub of a few
    /// metres between two nodes), else a new node, the edge keeping [0, s]
    /// and a new edge taking [s, end].
    int Split(int e, int s);

    /// Cuts edges at the requested (edge, station) places, highest station
    /// first on each edge so that lower ones stay valid.
    /// @return the node of each request, in request order
    std::vector<int> SplitAll(const std::vector<std::pair<int, int>> &requests);

    /// Moves each station to the mean of the points matched to it.
    void Average();

    /// Shortest path from node `from` to node `to` (Dijkstra on segment
    /// lengths), as steps along edges.
    /// @return the steps, or none when no path is shorter than `max_length`
    std::vector<openrider::sdk::RouteStep> ShortestPath(int from, int to, double max_length) const;

private:
    /// Grid key of cell (x, y).
    static int64_t Pack(int64_t x, int64_t y) { return (x << 32) ^ (y & 0xffffffffLL); }

    /// Grid key of the cell holding `q`.
    int64_t Key(const openrider::sdk::XY &q) const;

    double cell_;  ///< index grid cell (m)
    std::unordered_map<int64_t, std::vector<std::pair<int, int>>> index_;  ///< cell -> (edge, station)
};

} // namespace matching
