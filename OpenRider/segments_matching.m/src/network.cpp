#include "matching/network.h"

#include "matching/costs.h"

#include <algorithm>

namespace matching {

using openrider::sdk::Dist;
using openrider::sdk::Length;
using openrider::sdk::XY;

void Network::Reindex()
{
    index_.clear();
    for (int e = 0; e < static_cast<int>(edges.size()); ++e)
        for (int s = 0; s < static_cast<int>(edges[static_cast<size_t>(e)].stations.size()); ++s)
            index_[Key(edges[static_cast<size_t>(e)].stations[static_cast<size_t>(s)])].push_back({e, s});
}

XY Network::Tangent(int e, int s) const
{
    const std::vector<XY> &st = edges[static_cast<size_t>(e)].stations;
    const XY &a = st[static_cast<size_t>(std::max(0, s - 1))];
    const XY &b = st[std::min(st.size() - 1, static_cast<size_t>(s + 1))];
    return {b.x - a.x, b.y - a.y};
}

int Network::AddNode(const XY &q)
{
    nodes.push_back(q);
    return static_cast<int>(nodes.size()) - 1;
}

int Network::NodeAt(const XY &q, double radius)
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

int Network::Split(int e, int s)
{
    Edge &edge = edges[static_cast<size_t>(e)];
    const int last = static_cast<int>(edge.stations.size()) - 1;
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

std::vector<int> Network::SplitAll(const std::vector<std::pair<int, int>> &requests)
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
        nodes[k] = Split(requests[k].first, requests[k].second);
    return nodes;
}


void Network::Average()
{
    for (Edge &edge : edges)
        for (size_t s = 0; s < edge.stations.size(); ++s)
            if (edge.weights[s] > 0.0)
                edge.stations[s] = {edge.sums[s].x / edge.weights[s], edge.sums[s].y / edge.weights[s]};
}

std::vector<openrider::sdk::RouteStep> Network::ShortestPath(int from, int to, double max_length) const
{
    const size_t count = nodes.size();
    std::vector<double> distance(count, kInfinity);
    std::vector<openrider::sdk::RouteStep> via(count, {-1, true});  // step arriving at each node
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
        for (size_t e = 0; e < edges.size(); ++e) {
            const Edge &edge = edges[e];
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
    std::vector<openrider::sdk::RouteStep> path;
    for (int node = to; node != from;) {
        const openrider::sdk::RouteStep step = via[static_cast<size_t>(node)];
        path.push_back(step);
        const Edge &edge = edges[static_cast<size_t>(step.segment)];
        node = step.forward ? edge.from : edge.to;
    }
    std::reverse(path.begin(), path.end());
    return path;
}


int64_t Network::Key(const XY &q) const
{
    return Pack(static_cast<int64_t>(std::floor(q.x / cell_)), static_cast<int64_t>(std::floor(q.y / cell_)));
}

} // namespace matching
