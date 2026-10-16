// The segments plugin entry point: the heat-map algorithm (heatmap.h)
// behind the plugin ABI (openrider/segments.h).

#include <openrider/plugin.h>
#include <openrider/sdk/params.h>
#include <openrider/sdk/route_ranges.h>
#include <openrider/sdk/segment_exchange.h>
#include <openrider/segments.h>

#include "heatmap/heatmap.h"

#include <cstdio>
#include <string>

namespace {

// Indices into kParams.
enum Param { TraceWidth, CellSize, RestWindow, RestRadius, ParamCount };

const or_filter_param kParams[ParamCount] = {
    {"trace_width_m", "Trace width (m)", 8.0, 1.0, 50.0},
    {"cell_m", "Grid cell (m)", 8.0, 2.0, 16.0},
    {"rest_window_s", "Rest: time at a place (s)", 120.0, 30.0, 600.0},
    {"rest_radius_m", "Rest: place radius (m), 0 = none", 30.0, 0.0, 100.0},
};

// Builds the network of `tracks` with the declared parameters `values`.
int HeatmapBuild(const or_segment_track *tracks, size_t count, const double *values, or_segment_cancel_cb cancel,
                 void *user, or_segment_network **out, char *err, size_t errlen)
{
    if (out)
        *out = nullptr;
    std::string problem = !out ? "missing output" : openrider::sdk::TracksProblem(tracks, count);
    if (problem.empty())
        problem = openrider::sdk::ParamsProblem(kParams, ParamCount, values);
    if (!problem.empty()) {
        if (err && errlen)
            std::snprintf(err, errlen, "heatmap: %s", problem.c_str());
        return 1;
    }
    const std::vector<openrider::sdk::RecordedTrack> recorded = openrider::sdk::ImportTracks(tracks, count);
    heatmap::SegmentParams params;
    params.trace_width_m = values[TraceWidth];
    params.cell_m = values[CellSize];
    params.rest.window_s = values[RestWindow];
    params.rest.max_radius_m = values[RestRadius];
    bool stopped = false;
    openrider::sdk::SegmentNetwork network =
        heatmap::BuildNetwork(recorded, params, [&] { return stopped = stopped || (cancel && cancel(user) != 0); });
    if (stopped)
        return OR_SEGMENTS_CANCELLED;  // the network is incomplete
    openrider::sdk::AssignStepRanges(network, recorded);
    *out = openrider::sdk::ExportNetwork(network, recorded);
    return 0;
}

void HeatmapFree(or_segment_network *network) { openrider::sdk::FreeExportedNetwork(network); }

const or_segments_vtable kVtable = {sizeof(or_segments_vtable), "heatmap", "Heat map of the tracks", ParamCount, kParams, HeatmapBuild,
                                    HeatmapFree};
const or_plugin_info kInfo = {OR_PLUGIN_API_VERSION, OR_PLUGIN_API_MINOR, OR_PLUGIN_SEGMENTS, OR_PLUGIN_THREAD_SAFE, "heatmap", "0.1", &kVtable};

} // namespace

extern "C" OR_PLUGIN_EXPORT const or_plugin_info *openrider_plugin(void)
{
    return &kInfo;
}
