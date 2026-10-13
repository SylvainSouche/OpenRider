#include "matching/matching.h"

#include "core/local_projection.h"
#include "core/network_cleanup.h"
#include "core/plane_geometry.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <unordered_map>

namespace matching {

namespace {

using core::Dist;
using core::Length;
using core::XY;

constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr double kPi = 3.14159265358979323846;

// Costs of the matching, in units of a point at the tolerance distance.
constexpr double kOffCost = 1.0;       // a point off the network
constexpr double kSwitchCost = 2.0;    // entering or leaving the network
constexpr double kNodeCost = 0.5;      // passing from a segment to the next
constexpr double kTurnCost = 3.0;      // turning back on a segment
constexpr int kEndStations = 3;        // "at the end" of a segment: within this many stations
constexpr int kSmoothStations = 4;     // final lines smoothed over +/- this many stations

// A segment under construction: its stations from `from` to `to`, the sum
// and number of the track points matched to each (their mean is the
// station's position), and the tracks following it.
struct Edge {
    int from = -1, to = -1;
    std::vector<XY> stations;
    std::vector<XY> sums;
    std::vector<double> weights;
    std::set<int> routes;
};

// Where a point is: on a segment (edge, station, direction of travel along
// it: +1 towards `to`), or off the network (edge -1).
struct State {
    int edge = -1;
    int station = -1;
    int direction = 0;
    double emission = kOffCost;  // cost of the point being there
};

// The network under construction, with a grid index of its stations.
class Network {
public:
    explicit Network(double cell) : cell_(cell) {}

    std::vector<XY> nodes;
    std::vector<Edge> edges;

    // Rebuilds the station index (after stations moved or were added).
    void Reindex()
    {
        index_.clear();
        for (int e = 0; e < static_cast<int>(edges.size()); ++e)
            for (int s = 0; s < static_cast<int>(edges[static_cast<size_t>(e)].stations.size()); ++s)
                index_[Key(edges[static_cast<size_t>(e)].stations[static_cast<size_t>(s)])].push_back({e, s});
    }

    // Stations within `radius` of `q`: (edge, station) pairs.
    template <class Visit>
    void Near(const XY &q, double radius, Visit visit) const
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
                    if (Dist(edges[static_cast<size_t>(e)].stations[static_cast<size_t>(s)], q) <= radius)
                        visit(e, s);
            }
    }

    // Direction of edge `e` at station `s` (not normalised).
    XY Tangent(int e, int s) const
    {
        const std::vector<XY> &st = edges[static_cast<size_t>(e)].stations;
        const XY &a = st[static_cast<size_t>(std::max(0, s - 1))];
        const XY &b = st[std::min(st.size() - 1, static_cast<size_t>(s + 1))];
        return {b.x - a.x, b.y - a.y};
    }

    int AddNode(const XY &q)
    {
        nodes.push_back(q);
        return static_cast<int>(nodes.size()) - 1;
    }

    // An existing node within `radius` of `q`, or a new one there.
    int NodeAt(const XY &q, double radius)
    {
        int best = -1;
        double best_distance = radius;
        for (int n = 0; n < static_cast<int>(nodes.size()); ++n)
            if (Dist(nodes[static_cast<size_t>(n)], q) <= best_distance) {
                best_distance = Dist(nodes[static_cast<size_t>(n)], q);
                best = n;
            }
        return best >= 0 ? best : AddNode(q);
    }

    // Cuts edge `e` at station `s` and returns the node there: its end node
    // when `s` is within kEndStations of an end, else a new node, the edge keeping [0, s] and a
    // new edge taking [s, end].
    int Split(int e, int s)
    {
        Edge &edge = edges[static_cast<size_t>(e)];
        const int last = static_cast<int>(edge.stations.size()) - 1;
        // Close to an end, the end node is the junction: no stub of a few
        // metres between two nodes.
        if (s <= kEndStations)
            return edge.from;
        if (s >= last - kEndStations)
            return edge.to;
        const int node = AddNode(edge.stations[static_cast<size_t>(s)]);
        Edge tail;
        tail.from = node;
        tail.to = edge.to;
        tail.stations.assign(edge.stations.begin() + s, edge.stations.end());
        tail.sums.assign(edge.sums.begin() + s, edge.sums.end());
        tail.weights.assign(edge.weights.begin() + s, edge.weights.end());
        tail.routes = edge.routes;
        edge.stations.resize(static_cast<size_t>(s) + 1);
        edge.sums.resize(static_cast<size_t>(s) + 1);
        edge.weights.resize(static_cast<size_t>(s) + 1);
        edge.to = node;
        edges.push_back(std::move(tail));
        return node;
    }

    // Stations moved to the mean of the points matched to them.
    void Average()
    {
        for (Edge &edge : edges)
            for (size_t s = 0; s < edge.stations.size(); ++s)
                if (edge.weights[s] > 0.0)
                    edge.stations[s] = {edge.sums[s].x / edge.weights[s], edge.sums[s].y / edge.weights[s]};
    }

private:
    static int64_t Pack(int64_t x, int64_t y) { return (x << 32) ^ (y & 0xffffffffLL); }
    int64_t Key(const XY &q) const
    {
        return Pack(static_cast<int64_t>(std::floor(q.x / cell_)), static_cast<int64_t>(std::floor(q.y / cell_)));
    }

    double cell_;
    std::unordered_map<int64_t, std::vector<std::pair<int, int>>> index_;
};

// Each point of `line` replaced by the mean of the points within `half`
// stations of it (fewer near the ends): neighbouring stations are means of
// different subsets of the tracks and jitter across the path.
std::vector<XY> Smoothed(const std::vector<XY> &line, int half)
{
    std::vector<XY> out(line.size());
    for (size_t i = 0; i < line.size(); ++i) {
        const int reach = std::min<int>(half, static_cast<int>(std::min(i, line.size() - 1 - i)));
        XY sum;
        for (int k = -reach; k <= reach; ++k) {
            sum.x += line[static_cast<size_t>(static_cast<int>(i) + k)].x;
            sum.y += line[static_cast<size_t>(static_cast<int>(i) + k)].y;
        }
        out[i] = {sum.x / (2 * reach + 1), sum.y / (2 * reach + 1)};
    }
    return out;
}

// Directions of travel along `line`: from the previous point to the next.
std::vector<XY> Headings(const std::vector<XY> &line)
{
    std::vector<XY> headings(line.size());
    for (size_t i = 0; i < line.size(); ++i) {
        const XY &a = line[i > 0 ? i - 1 : i];
        const XY &b = line[i + 1 < line.size() ? i + 1 : i];
        headings[i] = {b.x - a.x, b.y - a.y};
    }
    return headings;
}

// Indices where `line` (evenly spaced every `step`) turns back on itself:
// `arm` metres before and after, it is within `close` of itself while the
// point stands out from both by more than `spike`. One index per turn.
std::vector<size_t> TurnArounds(const std::vector<XY> &line, double step, double arm, double close, double spike)
{
    std::vector<size_t> turns;
    const size_t reach = static_cast<size_t>(std::ceil(arm / step));
    size_t spike_end = 0;
    double turn_height = 0.0;
    for (size_t i = reach; i + reach < line.size(); ++i) {
        const XY &before = line[i - reach], &after = line[i + reach];
        if (Dist(before, after) >= close || std::min(Dist(line[i], before), Dist(line[i], after)) <= spike)
            continue;
        const double height = Dist(line[i], {(before.x + after.x) / 2.0, (before.y + after.y) / 2.0});
        if (!turns.empty() && i - reach < spike_end) {
            if (height > turn_height) {
                turns.back() = i;
                turn_height = height;
            }
        } else {
            turns.push_back(i);
            turn_height = height;
        }
        spike_end = i + reach;
    }
    return turns;
}

// The cheapest sequence of states for the points of `line` (a hidden
// Markov model solved by dynamic programming): candidates of each point are
// the nearest station of every segment within tolerance whose direction
// agrees with the point's, plus "off the network".
std::vector<State> Match(const Network &network, const std::vector<XY> &line, const MatchParams &params)
{
    const std::vector<XY> headings = Headings(line);
    const double min_cos = std::cos(params.max_angle_deg * kPi / 180.0);
    std::vector<std::vector<State>> candidates(line.size());
    for (size_t i = 0; i < line.size(); ++i) {
        std::map<int, State> nearest;  // per edge
        const XY &heading = headings[i];
        const double heading_length = std::hypot(heading.x, heading.y);
        network.Near(line[i], params.tolerance_m, [&](int e, int s) {
            const XY tangent = network.Tangent(e, s);
            const double tangent_length = std::hypot(tangent.x, tangent.y);
            if (heading_length <= 0.0 || tangent_length <= 0.0)
                return;
            const double cosine = (heading.x * tangent.x + heading.y * tangent.y) / (heading_length * tangent_length);
            if (std::fabs(cosine) < min_cos)
                return;  // moving across the segment, not along it
            const double distance = Dist(line[i], network.edges[static_cast<size_t>(e)].stations[static_cast<size_t>(s)]);
            const double emission = (distance / params.tolerance_m) * (distance / params.tolerance_m);
            auto found = nearest.find(e);
            if (found == nearest.end() || emission < found->second.emission)
                nearest[e] = State{e, s, cosine > 0.0 ? 1 : -1, emission};
        });
        candidates[i].push_back(State{});  // off the network
        for (const auto &entry : nearest)
            candidates[i].push_back(entry.second);
    }
    auto transition = [&](const State &a, const State &b) {
        if (a.edge < 0 && b.edge < 0)
            return 0.0;
        if (a.edge < 0 || b.edge < 0)
            return kSwitchCost;
        if (a.edge == b.edge) {
            const int advance = (b.station - a.station) * a.direction;
            if (b.direction == a.direction)
                return advance >= -1 && advance <= 4 ? 0.5 * (advance - 1) * (advance - 1) : kInfinity;
            return std::abs(b.station - a.station) <= 2 ? kTurnCost : kInfinity;
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
                const double total =
                    cost[i - 1][a] + transition(candidates[i - 1][a], candidates[i][b]) + candidates[i][b].emission;
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

// Resampled every `step` metres.
std::vector<XY> Stations(const std::vector<XY> &line, double step)
{
    const double length = Length(line);
    return core::Resampled(line, std::max<size_t>(2, static_cast<size_t>(std::ceil(length / step)) + 1));
}

// Cuts edges at the requested (edge, station) places, highest station
// first on each edge so that lower ones stay valid; returns the node of
// each request, in request order.
std::vector<int> SplitAll(Network &network, const std::vector<std::pair<int, int>> &requests)
{
    std::vector<size_t> order(requests.size());
    for (size_t k = 0; k < order.size(); ++k)
        order[k] = k;
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return requests[a].first != requests[b].first ? requests[a].first < requests[b].first
                                                      : requests[a].second > requests[b].second;
    });
    std::vector<int> nodes(requests.size(), -1);
    for (size_t k : order)
        nodes[k] = network.Split(requests[k].first, requests[k].second);
    return nodes;
}

// A stretch off the network (points first..last of `pass`) that mostly
// runs within twice the tolerance of existing segments is GPS drift along a
// known path, not a new path: 80% of its points have a station that close.
bool Alongside(const Network &network, const std::vector<XY> &pass, size_t first, size_t last, const MatchParams &params)
{
    size_t near = 0;
    for (size_t i = first; i <= last; ++i) {
        bool found = false;
        network.Near(pass[i], 2.0 * params.tolerance_m, [&](int, int) { found = true; });
        near += found ? 1 : 0;
    }
    return static_cast<double>(near) >= 0.8 * static_cast<double>(last - first + 1);
}

// Matches one pass of route `route` into the network: matched points are
// accumulated into their stations, stretches off the network at least
// min_new_m long and not alongside it (Alongside()) become new segments, cutting the segments where they
// leave and rejoin it.
void Insert(Network &network, const std::vector<XY> &pass, int route, const MatchParams &params)
{
    const std::vector<State> path = Match(network, pass, params);
    for (size_t i = 0; i < pass.size(); ++i) {
        const State &state = path[i];
        if (state.edge < 0)
            continue;
        Edge &edge = network.edges[static_cast<size_t>(state.edge)];
        edge.sums[static_cast<size_t>(state.station)].x += pass[i].x;
        edge.sums[static_cast<size_t>(state.station)].y += pass[i].y;
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
        if (static_cast<double>(j - i) * params.station_m >= params.min_new_m && !Alongside(network, pass, i, j, params))
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
    const std::vector<int> junctions = SplitAll(network, requests);
    for (size_t k = 0; k < stretches.size(); ++k) {
        const XY &first = pass[stretches[k].first], &last = pass[stretches[k].last];
        const int from = start_request[k] >= 0 ? junctions[static_cast<size_t>(start_request[k])]
                                               : network.NodeAt(first, 2.0 * params.tolerance_m);
        const int to = end_request[k] >= 0 ? junctions[static_cast<size_t>(end_request[k])]
                                           : network.NodeAt(last, 2.0 * params.tolerance_m);
        std::vector<XY> line;
        line.push_back(network.nodes[static_cast<size_t>(from)]);
        line.insert(line.end(), pass.begin() + static_cast<long>(stretches[k].first),
                    pass.begin() + static_cast<long>(stretches[k].last) + 1);
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

// Shortest path from node `from` to node `to` through the network
// (Dijkstra on segment lengths), as steps; empty when none is shorter than
// `max_length`.
std::vector<core::RouteStep> ShortestPath(const Network &network, int from, int to, double max_length)
{
    const size_t count = network.nodes.size();
    std::vector<double> distance(count, kInfinity);
    std::vector<core::RouteStep> via(count, {-1, true});  // step arriving at each node
    std::vector<bool> done(count, false);
    distance[static_cast<size_t>(from)] = 0.0;
    for (;;) {
        // The nearest node not yet settled (networks are small: no heap).
        int current = -1;
        for (size_t n = 0; n < count; ++n)
            if (!done[n] && distance[n] < kInfinity && (current < 0 || distance[n] < distance[static_cast<size_t>(current)]))
                current = static_cast<int>(n);
        if (current < 0 || distance[static_cast<size_t>(current)] > max_length)
            return {};
        if (current == to)
            break;
        done[static_cast<size_t>(current)] = true;
        for (size_t e = 0; e < network.edges.size(); ++e) {
            const Edge &edge = network.edges[e];
            for (bool forward : {true, false}) {
                if ((forward ? edge.from : edge.to) != current)
                    continue;
                const int other = forward ? edge.to : edge.from;
                const double through = distance[static_cast<size_t>(current)] + Length(edge.stations);
                if (through < distance[static_cast<size_t>(other)]) {
                    distance[static_cast<size_t>(other)] = through;
                    via[static_cast<size_t>(other)] = {static_cast<int>(e), forward};
                }
            }
        }
    }
    std::vector<core::RouteStep> path;
    for (int node = to; node != from;) {
        const core::RouteStep step = via[static_cast<size_t>(node)];
        path.push_back(step);
        const Edge &edge = network.edges[static_cast<size_t>(step.segment)];
        node = step.forward ? edge.from : edge.to;
    }
    std::reverse(path.begin(), path.end());
    return path;
}

} // namespace

// @impl 0f87-6ace-2f7b-f4d7
core::SegmentNetwork BuildNetwork(const std::vector<core::OverviewTrack> &tracks, const MatchParams &params,
                                  const std::function<bool()> &cancelled)
{
    core::SegmentNetwork result;
    if (tracks.empty())
        return result;
    // Local projection around the tracks' centre.
    double west = 1e9, east = -1e9, south = 1e9, north = -1e9;
    for (const core::OverviewTrack &track : tracks)
        for (size_t i = 0; i < track.lat.size(); ++i) {
            west = std::min(west, track.lon[i]);
            east = std::max(east, track.lon[i]);
            south = std::min(south, track.lat[i]);
            north = std::max(north, track.lat[i]);
        }
    const core::LocalProjection projection((west + east) / 2.0, (south + north) / 2.0);
    std::vector<std::vector<XY>> lines;
    for (const core::OverviewTrack &track : tracks) {
        std::vector<XY> line;
        for (size_t i = 0; i < track.lat.size(); ++i)
            line.push_back(projection.Forward(track.lon[i], track.lat[i]));
        lines.push_back(line.size() >= 2 ? Stations(line, params.station_m) : line);
    }

    // 1-3. Passes inserted one after the other.
    Network network(std::max(10.0, params.tolerance_m));
    const double arm = std::max(80.0, 4.0 * params.tolerance_m);
    for (size_t t = 0; t < lines.size(); ++t) {
        if (cancelled && cancelled())
            return result;
        const std::vector<XY> &line = lines[t];
        if (line.size() < 2)
            continue;
        size_t start = 0;
        std::vector<size_t> cuts = TurnArounds(line, params.station_m, arm, std::max(25.0, params.tolerance_m), 40.0);
        cuts.push_back(line.size() - 1);
        for (size_t cut : cuts) {
            if (cut <= start)
                continue;
            const std::vector<XY> pass(line.begin() + static_cast<long>(start), line.begin() + static_cast<long>(cut) + 1);
            Insert(network, pass, static_cast<int>(t), params);
            start = cut;
        }
    }

    // 4. Segments cut where a track turns back mid-segment. The network is
    // built: tracks are now matched back to it with twice the tolerance, so
    // that a stretch of drift does not drop a track off its path.
    if (cancelled && cancelled())
        return result;
    MatchParams final_params = params;
    final_params.tolerance_m = 2.0 * params.tolerance_m;
    std::vector<std::pair<int, int>> turn_cuts;
    for (const std::vector<XY> &line : lines) {
        const std::vector<State> path = Match(network, line, final_params);
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
    const int merge_stations = static_cast<int>(std::ceil(params.tolerance_m / params.station_m));
    for (const auto &cut : turn_cuts)
        if (distinct.empty() || distinct.back().first != cut.first || cut.second - distinct.back().second > merge_stations)
            distinct.push_back(cut);
    SplitAll(network, distinct);
    network.Reindex();

    // Each track's chain of segments.
    std::vector<core::NetworkRoute> routes;
    std::vector<std::set<int>> users(network.edges.size());
    for (size_t t = 0; t < lines.size(); ++t) {
        core::NetworkRoute route;
        route.id = tracks[t].id;
        for (const State &state : Match(network, lines[t], final_params)) {
            if (state.edge < 0)
                continue;
            const bool forward = state.direction > 0;
            if (!route.steps.empty() && route.steps.back().segment == state.edge && route.steps.back().forward == forward)
                continue;
            route.steps.push_back({state.edge, forward});
            users[static_cast<size_t>(state.edge)].insert(static_cast<int>(t));
        }
        routes.push_back(std::move(route));
    }

    // Nodes closer than the tolerance are one junction: tracks leaving a
    // path a few metres apart each cut it. The first of each group stays.
    std::vector<int> merged_into(network.nodes.size());
    for (size_t n = 0; n < network.nodes.size(); ++n) {
        merged_into[n] = static_cast<int>(n);
        for (size_t m = 0; m < n; ++m)
            if (merged_into[m] == static_cast<int>(m) && Dist(network.nodes[n], network.nodes[m]) <= params.tolerance_m) {
                merged_into[n] = static_cast<int>(m);
                break;
            }
    }
    for (Edge &edge : network.edges) {
        edge.from = merged_into[static_cast<size_t>(edge.from)];
        edge.to = merged_into[static_cast<size_t>(edge.to)];
    }

    // Gaps in the chains: where a track's GPS left the network for a while,
    // its chain jumps from one segment's end to another's start. The way it
    // went is the shortest path between them through the network, unless
    // that path is much longer than the jump (then it went elsewhere).
    for (size_t t = 0; t < routes.size(); ++t) {
        std::vector<core::RouteStep> filled;
        for (const core::RouteStep &step : routes[t].steps) {
            if (!filled.empty()) {
                const Edge &previous = network.edges[static_cast<size_t>(filled.back().segment)];
                const Edge &next = network.edges[static_cast<size_t>(step.segment)];
                const int exit = filled.back().forward ? previous.to : previous.from;
                const int entry = step.forward ? next.from : next.to;
                if (exit != entry) {
                    const double jump = Dist(network.nodes[static_cast<size_t>(exit)], network.nodes[static_cast<size_t>(entry)]);
                    for (const core::RouteStep &between : ShortestPath(network, exit, entry, 2.0 * jump + 100.0)) {
                        filled.push_back(between);
                        users[static_cast<size_t>(between.segment)].insert(static_cast<int>(t));
                    }
                }
            }
            filled.push_back(step);
        }
        routes[t].steps = std::move(filled);
    }

    // Output: every node, every segment some track uses.
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
        for (int t : users[e])
            segment.route_ids.push_back(tracks[static_cast<size_t>(t)].id);
        result.segments.push_back(std::move(segment));
    }
    result.routes = std::move(routes);

    // 5. Shared clean-up (removes the unused segments and nodes too).
    core::CleanupParams cleanup;
    cleanup.min_turn_m = arm;
    cleanup.max_loop_m = 150.0;
    cleanup.duplicate_distance_m = std::max(25.0, 1.5 * params.tolerance_m);
    core::CleanUpNetwork(result, cleanup);
    return result;
}

} // namespace matching
