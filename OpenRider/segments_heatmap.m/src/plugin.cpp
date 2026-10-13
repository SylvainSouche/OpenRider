// The segments plugin entry point: the heat-map algorithm (heatmap.h)
// behind the plugin ABI (openrider/segments.h).

#include <openrider/plugin.h>
#include <openrider/sdk/segment_exchange.h>
#include <openrider/segments.h>

#include "heatmap/heatmap.h"

#include <cstdio>

namespace {

// Indices into kParams.
enum Param { TraceWidth, CellSize, ParamCount };

const or_filter_param kParams[ParamCount] = {
    {"trace_width_m", "Trace width (m)", 8.0, 1.0, 50.0},
    {"cell_m", "Grid cell (m)", 8.0, 2.0, 16.0},
};

// Builds the network of `tracks` with the declared parameters `values`.
int HeatmapBuild(const or_segment_track *tracks, size_t count, const double *values, or_segment_cancel_cb cancel,
                 void *user, or_segment_network **out, char *err, size_t errlen)
{
    if (!out || (count > 0 && !tracks) || !values) {
        if (err && errlen)
            std::snprintf(err, errlen, "heatmap: missing arguments");
        return 1;
    }
    const std::vector<openrider::sdk::OverviewTrack> input = openrider::sdk::ImportTracks(tracks, count);
    heatmap::SegmentParams params;
    params.trace_width_m = values[TraceWidth];
    params.cell_m = values[CellSize];
    const openrider::sdk::SegmentNetwork network =
        heatmap::BuildNetwork(input, params, [&] { return cancel && cancel(user) != 0; });
    *out = openrider::sdk::ExportNetwork(network, input);
    return 0;
}

void HeatmapFree(or_segment_network *network) { openrider::sdk::FreeExportedNetwork(network); }

const or_segments_vtable kVtable = {"heatmap", "Heat map of the tracks", ParamCount, kParams, HeatmapBuild,
                                    HeatmapFree};
const or_plugin_info kInfo = {OR_PLUGIN_API_VERSION, OR_PLUGIN_SEGMENTS, "heatmap", "0.1", &kVtable};

} // namespace

extern "C" __attribute__((visibility("default"))) const or_plugin_info *openrider_plugin(void)
{
    return &kInfo;
}
