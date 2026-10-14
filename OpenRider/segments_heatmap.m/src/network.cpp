#include "heatmap/heatmap.h"

#include "heatmap/stages.h"

#include <openrider/sdk/network_cleanup.h>

#include <algorithm>
#include <cmath>

namespace heatmap {

// @impl 0f87-6acd-c829-c24f
SegmentNetwork BuildNetwork(const std::vector<RecordedTrack> &tracks, const SegmentParams &params,
                            const std::function<bool()> &cancelled)
{
    if (tracks.empty())
        return {};
    Assembly assembly{tracks, params, Analyse(tracks, params)};
    if (cancelled && cancelled())
        return {};  // superseded: the caller discards it
    // Entry points closer than the trace width (30 m at least; in skeleton
    // vertices) are one cut: within a wide trace, where a route enters or
    // leaves jitters across the whole width.
    assembly.cut_resolution_m = std::max(30.0, params.trace_width_m);
    assembly.margin = std::max(2, static_cast<int>(std::ceil(assembly.cut_resolution_m / params.cell_m)));
    // Route ends join a node within max(50 m, trace width).
    assembly.end_radius = std::max(50.0, params.trace_width_m);
    const size_t line_count = assembly.analysis.lines.size();
    assembly.travs.resize(tracks.size());
    assembly.cut_candidates.resize(line_count);
    assembly.cuts.resize(line_count);
    assembly.node_at.resize(line_count);
    assembly.proto_of.resize(line_count);
    assembly.items.resize(tracks.size());

    FindTraversals(assembly);
    CutSharedLines(assembly);
    PlaceNodes(assembly);
    MakeSharedSegments(assembly);
    ChainRoutes(assembly);
    BundleParts(assembly);
    ShapeSegments(assembly);
    ContractShortSegments(assembly);
    WriteNetwork(assembly);

    openrider::sdk::CleanupParams cleanup;
    cleanup.min_turn_m = std::max(80.0, 2.0 * params.trace_width_m);
    cleanup.max_loop_m = params.max_excursion_m;
    cleanup.duplicate_distance_m = std::max(25.0, params.trace_width_m);
    cleanup.kept_places = assembly.analysis.rest_places;
    cleanup.kept_radius_m = std::max(params.rest.max_radius_m, params.tolerance_m);
    openrider::sdk::CleanUpNetwork(assembly.net, cleanup);
    return std::move(assembly.net);
}

} // namespace heatmap
