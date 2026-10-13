#include "heatmap/stages.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace heatmap {

void ChainRoutes(Assembly &assembly)
{
    const std::vector<OverviewTrack> &tracks = assembly.tracks;
    const SegmentParams &params = assembly.params;
    const Analysis &analysis = assembly.analysis;
    const double cut_resolution_m = assembly.cut_resolution_m;
    std::vector<std::vector<Trav>> &travs = assembly.travs;
    std::vector<std::vector<int>> &cuts = assembly.cuts;
    std::vector<int> &parent = assembly.parent;
    std::vector<std::vector<int>> &node_at = assembly.node_at;
    std::vector<XY> &node_pos = assembly.node_pos;
    std::vector<Proto> &protos = assembly.protos;
    std::vector<std::vector<int>> &proto_of = assembly.proto_of;
    std::vector<std::vector<Item>> &items = assembly.items;
    std::map<std::pair<int, int>, int> &connectors = assembly.connectors;
    auto find = [&](int x) { return Find(assembly, x); };
    auto end_node = [&](const XY &q) { return EndNode(assembly, q); };
    // 5. Routes: shared traversals cover whole ranges (more than half of a
    // range overlapped); own parts become single-route segments joining the
    // nodes where the route leaves and rejoins the shared network.
    for (size_t t = 0; t < tracks.size(); ++t) {
        for (const Trav &tr : travs[t]) {
            if (tr.line < 0) {
                // Own part: geometry from the track; nodes filled below.
                Proto own;
                own.from = own.to = -1;
                own.own = true;
                for (size_t k = tr.first; k <= tr.last; ++k)
                    own.pts.push_back(analysis.xy[t][k]);
                own.routes.insert(t);
                if (tr.first < tr.last)
                    own.runs.push_back({t, tr.first, tr.last, true});
                items[t].push_back({static_cast<int>(protos.size()), true, -1, -1, tr.first, tr.last});
                protos.push_back(std::move(own));
                continue;
            }
            const size_t li = static_cast<size_t>(tr.line);
            const int lo = std::min(tr.from_v, tr.to_v), hi = std::max(tr.from_v, tr.to_v);
            const bool fwd = tr.to_v >= tr.from_v;
            std::vector<size_t> covered;
            for (size_t k = 0; k + 1 < cuts[li].size(); ++k) {
                const int a = cuts[li][k], b = cuts[li][k + 1];
                const int overlap = std::min(b, hi) - std::max(a, lo);
                if (overlap * 2 > b - a)
                    covered.push_back(k);
            }
            if (!fwd)
                std::reverse(covered.begin(), covered.end());
            for (size_t k : covered) {
                // The track's points along this range, for the geometry.
                size_t first = tr.last + 1, last = 0;
                const std::vector<Match> &m = analysis.matches[t];
                for (size_t i = tr.first; i <= tr.last; ++i)
                    if (m[i].line == tr.line && m[i].vertex >= cuts[li][k] && m[i].vertex <= cuts[li][k + 1]) {
                        first = std::min(first, i);
                        last = std::max(last, i);
                    }
                if (first < last)
                    protos[static_cast<size_t>(proto_of[li][k])].runs.push_back({t, first, last, fwd});
                else
                    first = tr.first, last = tr.last;
                Item it;
                it.first = first;
                it.last = last;
                it.proto = proto_of[li][k];
                it.forward = fwd;
                it.from = fwd ? node_at[li][k] : node_at[li][k + 1];
                it.to = fwd ? node_at[li][k + 1] : node_at[li][k];
                protos[static_cast<size_t>(it.proto)].routes.insert(t);
                items[t].push_back(it);
            }
        }
        // Junction crossings: going from one shared segment to another
        // directly, or through a short own stretch, passes one junction: the
        // exit and entry nodes are the same node, and the stretch is no
        // segment of its own -- provided the two nodes are within the cut
        // resolution; farther apart they are distinct junctions.
        std::vector<Item> &its = items[t];
        {
            std::vector<Item> kept_items;
            for (size_t i = 0; i < its.size(); ++i) {
                const Proto &pr = protos[static_cast<size_t>(its[i].proto)];
                const bool own = pr.from == -1 && its[i].from == -1;
                const bool between = !kept_items.empty() && kept_items.back().to >= 0 && i + 1 < its.size() &&
                                     its[i + 1].from >= 0;
                if (own && between && Length(pr.pts) <= params.max_excursion_m &&
                    Dist(node_pos[static_cast<size_t>(kept_items.back().to)],
                         node_pos[static_cast<size_t>(its[i + 1].from)]) <= cut_resolution_m) {
                    const int a = find(kept_items.back().to), b = find(its[i + 1].from);
                    if (a != b)
                        parent[static_cast<size_t>(b)] = a;
                    protos[static_cast<size_t>(its[i].proto)].routes.erase(t);
                    continue;
                }
                if (!own && !kept_items.empty() && kept_items.back().to >= 0 && its[i].from >= 0) {
                    const int exit = kept_items.back().to, entry = its[i].from;
                    if (Dist(node_pos[static_cast<size_t>(exit)], node_pos[static_cast<size_t>(entry)]) <= cut_resolution_m) {
                        const int a = find(exit), b = find(entry);
                        if (a != b)
                            parent[static_cast<size_t>(b)] = a;
                    } else if (find(exit) != find(entry)) {
                        // The two lines stop short of the junction between
                        // them: a connector joins them, shared by every
                        // route making that passage.
                        const std::pair<int, int> key{std::min(exit, entry), std::max(exit, entry)};
                        auto found = connectors.find(key);
                        if (found == connectors.end()) {
                            Proto link;
                            link.from = key.first;
                            link.to = key.second;
                            link.pts = {node_pos[static_cast<size_t>(key.first)], node_pos[static_cast<size_t>(key.second)]};
                            found = connectors.emplace(key, static_cast<int>(protos.size())).first;
                            protos.push_back(std::move(link));
                        }
                        // Its shape: the tracks' points between the two
                        // line ends (a hairpin tip, a junction's curve).
                        Proto &link = protos[static_cast<size_t>(found->second)];
                        link.routes.insert(t);
                        const size_t from_point = kept_items.back().last, to_point = its[i].first;
                        if (from_point < to_point)
                            link.runs.push_back({t, from_point, to_point, exit == key.first});
                        kept_items.push_back({found->second, exit == key.first, exit, entry, from_point, to_point});
                    }
                }
                kept_items.push_back(its[i]);
            }
            its.swap(kept_items);
        }
        // Own parts connect the previous step's exit to the next step's
        // entry (or a new node at the route's start or end).
        for (size_t i = 0; i < its.size(); ++i) {
            Proto &pr = protos[static_cast<size_t>(its[i].proto)];
            if (pr.from != -1 || pr.routes.size() != 1 || its[i].from != -1)
                continue;
            const int prev = i > 0 ? its[i - 1].to : -1;
            const int next = i + 1 < its.size() ? its[i + 1].from : -1;
            its[i].from = prev >= 0 ? prev : end_node(pr.pts.front());
            its[i].to = next >= 0 ? next : end_node(pr.pts.back());
            pr.from = its[i].from;
            pr.to = its[i].to;
        }
    }
}

} // namespace heatmap
