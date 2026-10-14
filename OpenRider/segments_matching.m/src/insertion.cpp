#include "matching/insertion.h"

#include "matching/track_geometry.h"
#include "matching/track_matching.h"

namespace matching {

using openrider::sdk::XY;

bool Alongside(const Network &network, const openrider::sdk::PlaneTrack &pass, size_t first, size_t last, const MatchParams &params)
{
    size_t near = 0;
    for (size_t i = first; i <= last; ++i) {
        bool found = false;
        network.Near(pass.points[i], 2.0 * params.tolerance_m, [&](int, int) { found = true; });
        near += found ? 1 : 0;
    }
    return static_cast<double>(near) >= 0.8 * static_cast<double>(last - first + 1);
}


void InsertPass(Network &network, const openrider::sdk::PlaneTrack &pass, int route, const MatchParams &params)
{
    const std::vector<State> path = MatchTrack(network, pass, params);
    for (size_t i = 0; i < pass.size(); ++i) {
        const State &state = path[i];
        if (state.edge < 0)
            continue;
        Edge &edge = network.edges[static_cast<size_t>(state.edge)];
        edge.sums[static_cast<size_t>(state.station)].x += pass.points[i].x;
        edge.sums[static_cast<size_t>(state.station)].y += pass.points[i].y;
        edge.weights[static_cast<size_t>(state.station)] += 1.0;
        edge.routes.insert(route);
    }
    // Stretches off the network, long enough to be a path of their own.
    struct Stretch {
        size_t first, last;
    };
    std::vector<Stretch> stretches;
    for (size_t i = 0; i < pass.size();) {
        if (path[i].edge >= 0) {
            ++i;
            continue;
        }
        size_t j = i;
        while (j + 1 < pass.size() && path[j + 1].edge < 0)
            ++j;
        if (pass.along[j] - pass.along[i] >= params.min_new_m && !Alongside(network, pass, i, j, params))
            stretches.push_back({i, j});
        i = j + 1;
    }
    // Junctions: where each stretch leaves and rejoins the network.
    std::vector<std::pair<int, int>> requests;
    std::vector<int> start_request(stretches.size(), -1), end_request(stretches.size(), -1);
    for (size_t k = 0; k < stretches.size(); ++k) {
        if (stretches[k].first > 0) {
            const State &before = path[stretches[k].first - 1];
            start_request[k] = static_cast<int>(requests.size());
            requests.push_back({before.edge, before.station});
        }
        if (stretches[k].last + 1 < pass.size()) {
            const State &after = path[stretches[k].last + 1];
            end_request[k] = static_cast<int>(requests.size());
            requests.push_back({after.edge, after.station});
        }
    }
    const std::vector<int> junctions = network.SplitAll(requests);
    for (size_t k = 0; k < stretches.size(); ++k) {
        const XY &first = pass.points[stretches[k].first], &last = pass.points[stretches[k].last];
        const int from = start_request[k] >= 0 ? junctions[static_cast<size_t>(start_request[k])]
                                               : network.NodeAt(first, 2.0 * params.tolerance_m);
        const int to = end_request[k] >= 0 ? junctions[static_cast<size_t>(end_request[k])]
                                           : network.NodeAt(last, 2.0 * params.tolerance_m);
        std::vector<XY> line;
        line.push_back(network.nodes[static_cast<size_t>(from)]);
        line.insert(line.end(), pass.points.begin() + static_cast<long>(stretches[k].first),
                    pass.points.begin() + static_cast<long>(stretches[k].last) + 1);
        line.push_back(network.nodes[static_cast<size_t>(to)]);
        Edge edge;
        edge.from = from;
        edge.to = to;
        edge.stations = Stations(line, params.station_m);
        edge.sums = edge.stations;
        edge.weights.assign(edge.stations.size(), 1.0);
        edge.routes.insert(route);
        network.edges.push_back(std::move(edge));
    }
    network.Average();
    network.Reindex();
}


} // namespace matching
