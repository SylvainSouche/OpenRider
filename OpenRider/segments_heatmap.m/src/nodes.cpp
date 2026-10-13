#include "heatmap/stages.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace heatmap {

void PlaceNodes(Assembly &assembly)
{
    const SegmentParams &params = assembly.params;
    const Analysis &analysis = assembly.analysis;
    const std::vector<std::vector<XY>> &lines = assembly.analysis.lines;
    std::vector<std::vector<int>> &cuts = assembly.cuts;
    std::vector<int> &parent = assembly.parent;
    std::vector<std::vector<int>> &node_at = assembly.node_at;
    auto find = [&](int x) { return Find(assembly, x); };
    auto new_node = [&](const XY &q) { return NewNode(assembly, q); };
    // 3. Nodes. Line ends meeting at a skeleton junction are one node
    // (union by proximity); interior cuts are nodes of their own.
    std::vector<std::pair<int, XY>> ends;  // provisional node, skeleton position
    for (size_t li = 0; li < lines.size(); ++li) {
        if (!analysis.kept[li])
            continue;
        for (size_t k = 0; k < cuts[li].size(); ++k) {
            const int id = new_node(analysis.mean[li][static_cast<size_t>(cuts[li][k])]);
            node_at[li].push_back(id);
            if (k == 0)
                ends.push_back({id, lines[li].front()});
            if (k + 1 == cuts[li].size())
                ends.push_back({id, lines[li].back()});
        }
    }
    for (size_t i = 0; i < ends.size(); ++i)
        for (size_t j = i + 1; j < ends.size(); ++j)
            if (Dist(ends[i].second, ends[j].second) <= 2.0 * params.cell_m + 1.0) {
                const int a = find(ends[i].first), b = find(ends[j].first);
                if (a != b)
                    parent[static_cast<size_t>(b)] = a;
            }
}

void MakeSharedSegments(Assembly &assembly)
{
    const Analysis &analysis = assembly.analysis;
    const std::vector<std::vector<XY>> &lines = assembly.analysis.lines;
    std::vector<std::vector<int>> &cuts = assembly.cuts;
    std::vector<std::vector<int>> &node_at = assembly.node_at;
    std::vector<Proto> &protos = assembly.protos;
    std::vector<std::vector<int>> &proto_of = assembly.proto_of;
    // 4. Shared segments: one per range between consecutive cuts.
    for (size_t li = 0; li < lines.size(); ++li) {
        if (!analysis.kept[li])
            continue;
        for (size_t k = 0; k + 1 < cuts[li].size(); ++k) {
            Proto pr;
            pr.from = node_at[li][k];
            pr.to = node_at[li][k + 1];
            pr.pts.assign(analysis.mean[li].begin() + cuts[li][k], analysis.mean[li].begin() + cuts[li][k + 1] + 1);
            proto_of[li].push_back(static_cast<int>(protos.size()));
            protos.push_back(std::move(pr));
        }
    }
}

} // namespace heatmap
