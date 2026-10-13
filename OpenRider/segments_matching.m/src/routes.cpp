#include "matching/routes.h"

#include "matching/costs.h"
#include "matching/track_matching.h"

#include <algorithm>
#include <cmath>

namespace matching {

using openrider::sdk::Dist;
using openrider::sdk::XY;

void CutAtTurnArounds(Network &network, const std::vector<std::vector<XY>> &lines, const MatchParams &params,
                      double merge_m)
{
    std::vector<std::pair<int, int>> turn_cuts;
    for (const std::vector<XY> &line : lines) {
        const std::vector<State> path = MatchTrack(network, line, params);
        for (size_t i = 1; i < path.size(); ++i) {
            const State &a = path[i - 1], &b = path[i];
            if (a.edge < 0 || a.edge != b.edge || a.direction == b.direction)
                continue;
            const int last = static_cast<int>(network.edges[static_cast<size_t>(a.edge)].stations.size()) - 1;
            if (b.station > kEndStations && b.station < last - kEndStations)
                turn_cuts.push_back({b.edge, b.station});
        }
    }
    // One cut per place: requests on an edge closer than the tolerance merge.
    std::sort(turn_cuts.begin(), turn_cuts.end());
    std::vector<std::pair<int, int>> distinct;
    const int merge_stations = static_cast<int>(std::ceil(merge_m / params.station_m));
    for (const auto &cut : turn_cuts)
        if (distinct.empty() || distinct.back().first != cut.first || cut.second - distinct.back().second > merge_stations)
            distinct.push_back(cut);
    network.SplitAll(distinct);
    network.Reindex();
}

TrackChains ChainTracks(const Network &network, const std::vector<std::vector<XY>> &lines,
                        const std::vector<openrider::sdk::OverviewTrack> &tracks, const MatchParams &params)
{
    TrackChains chains;
    chains.users.resize(network.edges.size());
    for (size_t t = 0; t < lines.size(); ++t) {
        openrider::sdk::NetworkRoute route;
        route.id = tracks[t].id;
        for (const State &state : MatchTrack(network, lines[t], params)) {
            if (state.edge < 0)
                continue;
            const bool forward = state.direction > 0;
            if (!route.steps.empty() && route.steps.back().segment == state.edge && route.steps.back().forward == forward)
                continue;
            route.steps.push_back({state.edge, forward});
            chains.users[static_cast<size_t>(state.edge)].insert(static_cast<int>(t));
        }
        chains.routes.push_back(std::move(route));
    }
    return chains;
}

void MergeCloseNodes(Network &network, double distance_m)
{
    std::vector<int> merged_into(network.nodes.size());
    for (size_t n = 0; n < network.nodes.size(); ++n) {
        merged_into[n] = static_cast<int>(n);
        for (size_t m = 0; m < n; ++m)
            if (merged_into[m] == static_cast<int>(m) && Dist(network.nodes[n], network.nodes[m]) <= distance_m) {
                merged_into[n] = static_cast<int>(m);
                break;
            }
    }
    for (Edge &edge : network.edges) {
        edge.from = merged_into[static_cast<size_t>(edge.from)];
        edge.to = merged_into[static_cast<size_t>(edge.to)];
    }
}

void FillChainGaps(const Network &network, TrackChains &chains)
{
    for (size_t t = 0; t < chains.routes.size(); ++t) {
        std::vector<openrider::sdk::RouteStep> filled;
        for (const openrider::sdk::RouteStep &step : chains.routes[t].steps) {
            if (!filled.empty()) {
                const Edge &previous = network.edges[static_cast<size_t>(filled.back().segment)];
                const Edge &next = network.edges[static_cast<size_t>(step.segment)];
                const int exit = filled.back().forward ? previous.to : previous.from;
                const int entry = step.forward ? next.from : next.to;
                if (exit != entry) {
                    const double jump = Dist(network.nodes[static_cast<size_t>(exit)], network.nodes[static_cast<size_t>(entry)]);
                    for (const openrider::sdk::RouteStep &between : network.ShortestPath(exit, entry, 2.0 * jump + 100.0)) {
                        filled.push_back(between);
                        chains.users[static_cast<size_t>(between.segment)].insert(static_cast<int>(t));
                    }
                }
            }
            filled.push_back(step);
        }
        chains.routes[t].steps = std::move(filled);
    }
}

} // namespace matching
