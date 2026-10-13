#include "heatmap/analysis_stages.h"

#include "heatmap/skeleton.h"
#include "heatmap/turnarounds.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace heatmap {

std::unordered_set<Cell> Corridor(const Analysis &analysis, const SegmentParams &params)
{
    const std::vector<std::vector<XY>> &xy = analysis.xy;
    // @impl 0f87-6acd-60a9-7bf6
    // @impl 0f87-6acd-dc0b-a958
    // 2. Blurred heat map: each track drawn as a line trace_width_m wide.
    // Along the track, a disc of that diameter is stamped: 1 on the cells
    // whose centre lies inside it, 0.5 on a rim one cell wider (the blur),
    // once per track (the higher value); tracks add up. The cell of the
    // point itself always counts, so a width below the cell size still
    // draws a one-cell line. Discs are stamped every half radius at most,
    // which leaves the band covered to 97% of its half-width.
    const double cell = params.cell_m;
    const double radius = std::max(0.0, params.trace_width_m / 2.0);
    const double rim = radius + cell;
    const int reach = static_cast<int>(std::ceil(rim / cell));
    std::unordered_map<Cell, double> heat;
    std::unordered_map<Cell, int> reached;  // distinct tracks per cell
    std::vector<std::vector<XY>> passes;
    for (size_t t = 0; t < xy.size(); ++t) {
        size_t start = 0;
        for (size_t turn : analysis.turns[t]) {
            passes.emplace_back(xy[t].begin() + static_cast<long>(start), xy[t].begin() + static_cast<long>(turn) + 1);
            start = turn;
        }
        passes.emplace_back(xy[t].begin() + static_cast<long>(start), xy[t].end());
    }
    for (const std::vector<XY> &l : passes) {
        std::unordered_map<Cell, double> mine;
        for (const XY &q : Densify(l, std::max(cell * 0.5, radius * 0.5))) {
            const int64_t ix = static_cast<int64_t>(std::floor(q.x / cell));
            const int64_t iy = static_cast<int64_t>(std::floor(q.y / cell));
            mine[Key(ix, iy)] = 1.0;
            for (int dy = -reach; dy <= reach; ++dy)
                for (int dx = -reach; dx <= reach; ++dx) {
                    // Distance from the point to the centre of that cell.
                    const double distance = std::hypot((ix + dx + 0.5) * cell - q.x, (iy + dy + 0.5) * cell - q.y);
                    if (distance > rim)
                        continue;
                    double &v = mine[Key(ix + dx, iy + dy)];
                    v = std::max(v, distance <= radius ? 1.0 : 0.5);
                }
        }
        for (const auto &[c, v] : mine) {
            heat[c] += v;
            ++reached[c];
        }
    }

    // 3. Corridor: cells dense enough, reached by enough passes.
    std::unordered_set<Cell> corridor;
    for (const auto &[c, v] : heat)
        if (v >= params.min_density && reached[c] >= params.min_tracks)
            corridor.insert(c);
    return corridor;
}

void TraceLines(Analysis &analysis, const std::unordered_set<Cell> &corridor, const SegmentParams &params)
{
    const double cell = params.cell_m;
    // 4. Corridor components -> skeleton -> edges.
    std::vector<Edge> edges;
    int next_node = 0;
    std::unordered_set<Cell> seen;
    for (Cell start : corridor) {
        if (seen.count(start))
            continue;
        std::vector<Cell> comp;
        std::deque<Cell> q{start};
        seen.insert(start);
        while (!q.empty()) {
            const Cell c = q.front();
            q.pop_front();
            comp.push_back(c);
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    const Cell nb = Key(KeyX(c) + dx, KeyY(c) + dy);
                    if (corridor.count(nb) && seen.insert(nb).second)
                        q.push_back(nb);
                }
        }
        TraceComponent(comp, cell, edges, next_node);
    }
    CleanGraph(edges, params.min_spur_m);
    std::vector<std::vector<XY>> &lines = analysis.lines;  // densified skeleton per kept edge
    for (Edge &ed : edges)
        if (!ed.removed && Length(ed.pts) >= params.min_spur_m * 0.5)
            lines.push_back(Densify(Simplify(ed.pts, cell * 0.5), cell));
}

} // namespace heatmap
