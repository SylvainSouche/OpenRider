#include "heatmap/assembly.h"

namespace heatmap {

int Find(Assembly &assembly, int x)
{
    std::vector<int> &parent = assembly.parent;
    while (parent[static_cast<size_t>(x)] != x)
        x = parent[static_cast<size_t>(x)] = parent[static_cast<size_t>(parent[static_cast<size_t>(x)])];
    return x;
}

int NewNode(Assembly &assembly, const XY &q)
{
    assembly.parent.push_back(static_cast<int>(assembly.parent.size()));
    assembly.node_pos.push_back(q);
    return static_cast<int>(assembly.parent.size() - 1);
}

int EndNode(Assembly &assembly, const XY &q)
{
    int best = -1;
    double best_distance = assembly.end_radius;
    for (size_t n = 0; n < assembly.node_pos.size(); ++n) {
        const double distance = Dist(assembly.node_pos[n], q);
        if (distance <= best_distance) {
            best_distance = distance;
            best = Find(assembly, static_cast<int>(n));
        }
    }
    return best >= 0 ? best : NewNode(assembly, q);
}

} // namespace heatmap
