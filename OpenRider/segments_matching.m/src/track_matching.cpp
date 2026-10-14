#include "matching/track_matching.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <utility>

namespace matching {

namespace {

constexpr double kPi = 3.14159265358979323846;

} // namespace

using openrider::sdk::Dist;
using openrider::sdk::XY;

std::vector<State> MatchTrack(const Network &network, const openrider::sdk::PlaneTrack &track, const MatchParams &params)
{
    const std::vector<XY> &line = track.points;
    const double min_cos = std::cos(params.max_angle_deg * kPi / 180.0);
    std::vector<std::vector<State>> candidates(line.size());
    for (size_t i = 0; i < line.size(); ++i) {
        std::map<int, std::pair<double, State>> nearest;  // per edge: distance to the station, state
        const XY &heading = track.velocities[i];
        const double heading_length = std::hypot(heading.x, heading.y);
        // A station half a spacing farther may still have the line beside
        // the point within the tolerance.
        network.Near(line[i], params.tolerance_m + params.station_m, [&](int e, int s) {
            const XY tangent = network.Tangent(e, s);
            const double tangent_length = std::hypot(tangent.x, tangent.y);
            if (!(heading_length > 0.0) || tangent_length <= 0.0)
                return;
            const double cosine = (heading.x * tangent.x + heading.y * tangent.y) / (heading_length * tangent_length);
            if (std::fabs(cosine) < min_cos)
                return;  // moving across the segment, not along it
            // The station beside the point; scored by the distance across
            // the line there.
            const double to_station = Dist(line[i], network.edges[static_cast<size_t>(e)].stations[static_cast<size_t>(s)]);
            const auto found = nearest.find(e);
            if (found != nearest.end() && found->second.first <= to_station)
                return;
            const double distance = network.DistanceAcross(e, s, line[i]);
            const double emission = (distance / params.tolerance_m) * (distance / params.tolerance_m);
            nearest[e] = {to_station, State{e, s, cosine > 0.0 ? 1 : -1, emission}};
        });
        candidates[i].push_back(State{});  // off the network
        for (const auto &entry : nearest)
            if (entry.second.second.emission <= 1.0)  // within the tolerance across the line
                candidates[i].push_back(entry.second.second);
    }
    // `expected`: stations walked since the previous point.
    auto transition = [&](const State &a, const State &b, double expected) {
        if (a.edge < 0 && b.edge < 0)
            return 0.0;
        if (a.edge < 0 || b.edge < 0)
            return kSwitchCost;
        if (a.edge == b.edge) {
            const int advance = (b.station - a.station) * a.direction;
            if (b.direction == a.direction) {
                const double off = advance - expected, spread = std::max(1.0, expected);
                return advance >= -1 && advance <= 2.0 * expected + 3.0 ? 0.5 * off * off / spread : kInfinity;
            }
            return std::abs(b.station - a.station) <= 2 + static_cast<int>(expected) ? kTurnCost : kInfinity;
        }
        // From the end of a's segment (in its direction of travel) to the
        // start of b's, through their common node.
        const Edge &ea = network.edges[static_cast<size_t>(a.edge)];
        const Edge &eb = network.edges[static_cast<size_t>(b.edge)];
        const int last_a = static_cast<int>(ea.stations.size()) - 1, last_b = static_cast<int>(eb.stations.size()) - 1;
        const bool a_at_exit = a.direction > 0 ? a.station >= last_a - kEndStations : a.station <= kEndStations;
        const bool b_at_entry = b.direction > 0 ? b.station <= kEndStations : b.station >= last_b - kEndStations;
        const int exit_node = a.direction > 0 ? ea.to : ea.from;
        const int entry_node = b.direction > 0 ? eb.from : eb.to;
        return a_at_exit && b_at_entry && exit_node == entry_node ? kNodeCost : kInfinity;
    };
    // Forward pass: best cost of each candidate, and where it came from.
    std::vector<std::vector<double>> cost(line.size());
    std::vector<std::vector<int>> from(line.size());
    for (size_t i = 0; i < line.size(); ++i) {
        cost[i].assign(candidates[i].size(), kInfinity);
        from[i].assign(candidates[i].size(), -1);
        for (size_t b = 0; b < candidates[i].size(); ++b) {
            if (i == 0) {
                cost[i][b] = candidates[i][b].emission;
                continue;
            }
            for (size_t a = 0; a < candidates[i - 1].size(); ++a) {
                const double expected = (track.along[i] - track.along[i - 1]) / params.station_m;
                const double total = cost[i - 1][a] + transition(candidates[i - 1][a], candidates[i][b], expected) +
                                     candidates[i][b].emission;
                if (total < cost[i][b]) {
                    cost[i][b] = total;
                    from[i][b] = static_cast<int>(a);
                }
            }
        }
    }
    // Back-tracking from the cheapest last state.
    std::vector<State> path(line.size());
    if (line.empty())
        return path;
    int at = static_cast<int>(std::min_element(cost.back().begin(), cost.back().end()) - cost.back().begin());
    for (size_t i = line.size(); i-- > 0;) {
        path[i] = candidates[i][static_cast<size_t>(at)];
        at = from[i][static_cast<size_t>(at)];
        if (at < 0)
            at = 0;
    }
    return path;
}


} // namespace matching
