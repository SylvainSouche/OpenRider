#include "heatmap/stages.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace heatmap {

void WriteNetwork(Assembly &assembly)
{
    const std::vector<OverviewTrack> &tracks = assembly.tracks;
    const SegmentParams &params = assembly.params;
    const Analysis &analysis = assembly.analysis;
    std::vector<XY> &node_pos = assembly.node_pos;
    std::vector<Proto> &protos = assembly.protos;
    std::vector<std::vector<Item>> &items = assembly.items;
    SegmentNetwork &net = assembly.net;
    auto find = [&](int x) { return Find(assembly, x); };
    // 6. Output: final node ids (merged), segments used by a route, routes.
    // analysis node's position is the mean of its provisional members in use.
    std::set<int> used;
    for (const Proto &pr : protos)
        if (!pr.routes.empty() && pr.from >= 0 && pr.to >= 0) {
            used.insert(pr.from);
            used.insert(pr.to);
        }
    // A node's position is its medoid: the member position nearest all the
    // others. The centroid of nodes merged from two paths lies between them,
    // on neither; the medoid is on one.
    std::map<int, int> final_of_root;
    std::vector<std::vector<XY>> members;
    for (int prov : used) {
        const int root = find(prov);
        auto it = final_of_root.find(root);
        if (it == final_of_root.end()) {
            it = final_of_root.emplace(root, static_cast<int>(members.size())).first;
            members.emplace_back();
        }
        members[static_cast<size_t>(it->second)].push_back(node_pos[static_cast<size_t>(prov)]);
    }
    std::vector<XY> final_pos;
    for (const std::vector<XY> &group : members) {
        XY best = group.front();
        double best_sum = 1e300;
        for (const XY &candidate : group) {
            double sum = 0.0;
            for (const XY &other : group)
                sum += Dist(candidate, other);
            if (sum < best_sum) {
                best_sum = sum;
                best = candidate;
            }
        }
        final_pos.push_back(best);
    }
    auto node_id = [&](int prov) { return final_of_root[find(prov)]; };
    std::vector<int> seg_id(protos.size(), -1);
    for (size_t i = 0; i < protos.size(); ++i) {
        const Proto &pr = protos[i];
        if (pr.routes.empty() || pr.from < 0 || pr.to < 0)
            continue;
        NetworkSegment sg;
        sg.id = static_cast<int>(net.segments.size());
        sg.from = node_id(pr.from);
        sg.to = node_id(pr.to);
        std::vector<XY> pts = Simplify(pr.pts, params.cell_m * 0.3);
        // The line meets its nodes: ends moved onto them when close, or
        // extended to them.
        for (bool at_start : {true, false}) {
            const XY node = final_pos[static_cast<size_t>(at_start ? sg.from : sg.to)];
            XY &end = at_start ? pts.front() : pts.back();
            if (Dist(end, node) <= params.cell_m)
                end = node;
            else if (at_start)
                pts.insert(pts.begin(), node);
            else
                pts.push_back(node);
        }
        sg.length_m = Length(pts);
        for (const XY &q : pts) {
            double lon, lat;
            analysis.pr.Inv(q.x, q.y, lon, lat);
            sg.lon.push_back(lon);
            sg.lat.push_back(lat);
        }
        for (size_t t : pr.routes)
            sg.route_ids.push_back(tracks[t].id);
        seg_id[i] = sg.id;
        net.segments.push_back(std::move(sg));
    }
    for (size_t n = 0; n < final_pos.size(); ++n) {
        NetworkNode nd;
        nd.id = static_cast<int>(n);
        analysis.pr.Inv(final_pos[n].x, final_pos[n].y, nd.lon, nd.lat);
        net.nodes.push_back(nd);
    }
    for (size_t t = 0; t < tracks.size(); ++t) {
        NetworkRoute r;
        r.id = tracks[t].id;
        for (const Item &it : items[t]) {
            const int sid = seg_id[static_cast<size_t>(it.proto)];
            if (sid >= 0)
                r.steps.push_back({sid, it.forward});
        }
        net.routes.push_back(std::move(r));
    }
}

} // namespace heatmap
