#include "heatmap/stages.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace heatmap {

void ShapeSegments(Assembly &assembly)
{
    const SegmentParams &params = assembly.params;
    const Analysis &analysis = assembly.analysis;
    std::vector<Proto> &protos = assembly.protos;
    // 5b. Shape of the shared segments: the median of the tracks' runs
    // along each, compared at equal fractions of their length, between the
    // segment's two node positions. The heat-map line is a corridor's
    // centre: where a path bends tighter than the trace width (a
    // switchback), the corridor is a blob and its centre cuts the bends;
    // the tracks themselves keep them.
    for (Proto &pr : protos) {
        if (pr.runs.empty() || pr.pts.size() < 2)
            continue;
        std::vector<std::vector<XY>> shapes;
        double total_length = 0.0;
        for (const Run &run : pr.runs) {
            std::vector<XY> shape(analysis.xy[run.track].begin() + static_cast<long>(run.first),
                                  analysis.xy[run.track].begin() + static_cast<long>(run.last) + 1);
            if (!run.forward)
                std::reverse(shape.begin(), shape.end());
            total_length += Length(shape);
            shapes.push_back(std::move(shape));
        }
        const double mean_length = total_length / static_cast<double>(shapes.size());
        const size_t samples = std::max<size_t>(2, static_cast<size_t>(std::ceil(mean_length / params.cell_m)) + 1);
        std::vector<XY> shape(samples);
        std::vector<double> xs(shapes.size()), ys(shapes.size());
        for (size_t j = 0; j < samples; ++j) {
            const double fraction = static_cast<double>(j) / static_cast<double>(samples - 1);
            for (size_t r = 0; r < shapes.size(); ++r) {
                const XY q = AlongFrom(shapes[r], 0, fraction * Length(shapes[r]));
                xs[r] = q.x;
                ys[r] = q.y;
            }
            // Median per coordinate: a run's detour does not drag the shape.
            std::nth_element(xs.begin(), xs.begin() + static_cast<long>(xs.size() / 2), xs.end());
            std::nth_element(ys.begin(), ys.begin() + static_cast<long>(ys.size() / 2), ys.end());
            shape[j] = {xs[xs.size() / 2], ys[ys.size() / 2]};
        }
        shape.front() = pr.pts.front();  // anchored on the nodes
        shape.back() = pr.pts.back();
        pr.pts = std::move(shape);
    }
}

void ContractShortSegments(Assembly &assembly)
{
    const SegmentParams &params = assembly.params;
    std::vector<int> &parent = assembly.parent;
    std::vector<Proto> &protos = assembly.protos;
    auto find = [&](int x) { return Find(assembly, x); };
    // 6a. Nothing finer than the trace width can be told apart: a segment
    // shorter than it is a junction seen at too fine a scale. Its two ends
    // become one node and the segment goes; routes stay connected through
    // the merged node (their steps on it are dropped below).
    // A single-route loop back to its own node, shorter than an excursion,
    // is GPS wandering (a pause, a detour round an obstacle): it goes too.
    const double min_segment_m = std::max(params.trace_width_m, params.cell_m);
    for (Proto &pr : protos) {
        if (pr.routes.size() == 1 && pr.from >= 0 && find(pr.from) == find(pr.to) &&
            Length(pr.pts) < params.max_excursion_m) {
            pr.routes.clear();
            continue;
        }
        if (pr.routes.empty() || pr.from < 0 || pr.to < 0 || Length(pr.pts) >= min_segment_m)
            continue;
        const int a = find(pr.from), b = find(pr.to);
        if (a != b)
            parent[static_cast<size_t>(b)] = a;
        pr.routes.clear();
    }
}

} // namespace heatmap
