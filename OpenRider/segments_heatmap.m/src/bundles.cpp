#include "heatmap/stages.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace heatmap {

void BundleParts(Assembly &assembly)
{
    const SegmentParams &params = assembly.params;
    const double cut_resolution_m = assembly.cut_resolution_m;
    std::vector<int> &parent = assembly.parent;
    std::vector<Proto> &protos = assembly.protos;
    std::vector<std::vector<Item>> &items = assembly.items;
    auto find = [&](int x) { return Find(assembly, x); };
    // 5a. Bundles: outings that leave the shared network at the
    // same node and rejoin it at the same node along nearly the same way
    // followed one path the heat map could not draw (a switchback whose legs
    // are closer than the GPS spread). Own parts joining the same two nodes,
    // of similar length, whose shapes stay within max(25 m, trace width) of
    // each other on average, are one segment travelled by all of them. The
    // same holds for any two segments, shared ones included.
    {
        const double bundle_distance = std::max(25.0, params.trace_width_m);
        auto shape_of = [&](const Proto &pr, bool reversed) {
            const size_t samples = 20;
            std::vector<XY> line = pr.pts;
            if (reversed)
                std::reverse(line.begin(), line.end());
            const double length = Length(line);
            std::vector<XY> shape;
            for (size_t j = 0; j < samples; ++j)
                shape.push_back(AlongFrom(line, 0, length * static_cast<double>(j) / static_cast<double>(samples - 1)));
            return shape;
        };
        auto mean_distance = [](const std::vector<XY> &a, const std::vector<XY> &b) {
            double sum = 0.0;
            for (size_t j = 0; j < a.size(); ++j)
                sum += Dist(a[j], b[j]);
            return sum / static_cast<double>(a.size());
        };
        // Candidates: own parts between two nodes. Their ends need not be the
        // same nodes: runs leave and rejoin a path a little apart, so ends
        // within the cut resolution match, and merging joins those nodes.
        const double end_distance = cut_resolution_m;
        // Shared segments take part too: a heat-map line and a bundle of own
        // parts can describe the same path side by side. The busiest
        // segments come first and absorb the others.
        std::vector<size_t> candidates;
        for (size_t p = 0; p < protos.size(); ++p) {
            const Proto &pr = protos[p];
            if (!pr.routes.empty() && pr.from >= 0 && pr.to >= 0 && pr.pts.size() >= 2)
                candidates.push_back(p);
        }
        std::stable_sort(candidates.begin(), candidates.end(), [&](size_t a, size_t b) {
            return protos[a].routes.size() > protos[b].routes.size();
        });
        std::vector<int> merged_into(protos.size(), -1);
        std::vector<bool> merged_reversed(protos.size(), false);
        std::vector<size_t> bundles;  // representative of each bundle
        std::vector<std::vector<XY>> bundle_shapes;
        for (size_t p : candidates) {
            const Proto &pr = protos[p];
            const XY &start = pr.pts.front(), &end = pr.pts.back();
            bool joined = false;
            for (size_t b = 0; b < bundles.size() && !joined; ++b) {
                const Proto &rep = protos[bundles[b]];
                const double ratio = Length(pr.pts) / std::max(1.0, Length(rep.pts));
                if (ratio < 0.7 || ratio > 1.0 / 0.7)
                    continue;
                // Orientation: the pairing of ends that fits; both are tried
                // when the ends are close (a loop).
                for (bool reversed : {false, true}) {
                    const XY &a = reversed ? end : start, &z = reversed ? start : end;
                    if (Dist(a, rep.pts.front()) > end_distance || Dist(z, rep.pts.back()) > end_distance)
                        continue;
                    if (mean_distance(bundle_shapes[b], shape_of(pr, reversed)) > bundle_distance)
                        continue;
                    const size_t r = bundles[b];
                    merged_into[p] = static_cast<int>(r);
                    merged_reversed[p] = reversed;
                    Proto &target = protos[r];
                    for (const Run &run : pr.runs)
                        target.runs.push_back({run.track, run.first, run.last, run.forward != reversed});
                    target.routes.insert(pr.routes.begin(), pr.routes.end());
                    // The ends meet the representative's nodes.
                    const int pr_start = reversed ? pr.to : pr.from, pr_end = reversed ? pr.from : pr.to;
                    for (auto [mine, theirs] : {std::pair<int, int>{pr_start, target.from}, {pr_end, target.to}}) {
                        const int x = find(theirs), y = find(mine);
                        if (x != y)
                            parent[static_cast<size_t>(y)] = x;
                    }
                    joined = true;
                    break;
                }
            }
            if (!joined) {
                bundles.push_back(p);
                bundle_shapes.push_back(shape_of(pr, false));
            }
        }
        for (size_t p = 0; p < protos.size(); ++p)
            if (merged_into[p] >= 0)
                protos[p].routes.clear();
        for (std::vector<Item> &its : items)
            for (Item &it : its) {
                const int target = merged_into[static_cast<size_t>(it.proto)];
                if (target < 0)
                    continue;
                if (merged_reversed[static_cast<size_t>(it.proto)])
                    it.forward = !it.forward;
                it.proto = target;
            }
    }
}

} // namespace heatmap
