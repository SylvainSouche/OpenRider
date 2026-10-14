// The segments plugin entry point: track matching (matching.h) behind the
// plugin ABI (openrider/segments.h).

#include <openrider/plugin.h>
#include <openrider/sdk/overview.h>
#include <openrider/sdk/params.h>
#include <openrider/sdk/route_ranges.h>
#include <openrider/sdk/segment_exchange.h>
#include <openrider/segments.h>

#include "matching/matching.h"

#include <cstdio>
#include <string>

namespace {

// Indices into kParams.
enum Param { Tolerance, MinNew, MaxAngle, ParamCount };

const or_filter_param kParams[ParamCount] = {
    {"tolerance_m", "Matching distance (m)", 15.0, 3.0, 50.0},
    {"min_new_m", "Shortest new path (m)", 40.0, 10.0, 300.0},
    {"max_angle_deg", "Direction tolerance (deg)", 50.0, 15.0, 85.0},
};

// Builds the network of `tracks` with the declared parameters `values`.
int MatchingBuild(const or_segment_track *tracks, size_t count, const double *values, or_segment_cancel_cb cancel,
                  void *user, or_segment_network **out, char *err, size_t errlen)
{
    if (out)
        *out = nullptr;
    std::string problem = !out ? "missing output" : openrider::sdk::TracksProblem(tracks, count);
    if (problem.empty())
        problem = openrider::sdk::ParamsProblem(kParams, ParamCount, values);
    if (!problem.empty()) {
        if (err && errlen)
            std::snprintf(err, errlen, "matching: %s", problem.c_str());
        return 1;
    }
    const std::vector<openrider::sdk::RecordedTrack> recorded = openrider::sdk::ImportTracks(tracks, count);
    // The method still works on thinned tracks, as the application gave it
    // before; it moves to the recorded points next.
    std::vector<openrider::sdk::OverviewTrack> input;
    for (const openrider::sdk::RecordedTrack &track : recorded)
        input.push_back(openrider::sdk::ThinTrack(track.id, track.points, openrider::sdk::kOverviewSpacingM));
    matching::MatchParams params;
    params.tolerance_m = values[Tolerance];
    params.min_new_m = values[MinNew];
    params.max_angle_deg = values[MaxAngle];
    bool stopped = false;
    openrider::sdk::SegmentNetwork network =
        matching::BuildNetwork(input, params, [&] { return stopped = stopped || (cancel && cancel(user) != 0); });
    if (stopped)
        return OR_SEGMENTS_CANCELLED;  // the network is incomplete
    openrider::sdk::AssignStepRanges(network, recorded);
    *out = openrider::sdk::ExportNetwork(network, recorded);
    return 0;
}

void MatchingFree(or_segment_network *network) { openrider::sdk::FreeExportedNetwork(network); }

const or_segments_vtable kVtable = {sizeof(or_segments_vtable), "matching", "Matching the tracks", ParamCount, kParams, MatchingBuild,
                                    MatchingFree};
const or_plugin_info kInfo = {OR_PLUGIN_API_VERSION, OR_PLUGIN_API_MINOR, OR_PLUGIN_SEGMENTS, OR_PLUGIN_THREAD_SAFE, "matching", "0.1", &kVtable};

} // namespace

extern "C" __attribute__((visibility("default"))) const or_plugin_info *openrider_plugin(void)
{
    return &kInfo;
}
