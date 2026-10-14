#include "matching/matching.h"

#include "matching/insertion.h"
#include "matching/network.h"
#include "matching/output.h"
#include "matching/plane_track.h"
#include "matching/routes.h"
#include "matching/track_geometry.h"

#include <openrider/sdk/local_projection.h>
#include <openrider/sdk/network_cleanup.h>

#include <algorithm>
#include <cmath>

namespace matching {

namespace {

// Projection around the tracks' centre.
openrider::sdk::LocalProjection CentredProjection(const std::vector<openrider::sdk::RecordedTrack> &tracks)
{
    double west = 1e9, east = -1e9, south = 1e9, north = -1e9;
    for (const openrider::sdk::RecordedTrack &track : tracks)
        for (const or_point &point : track.points) {
            if (std::isnan(point.lat) || std::isnan(point.lon))
                continue;
            west = std::min(west, point.lon);
            east = std::max(east, point.lon);
            south = std::min(south, point.lat);
            north = std::max(north, point.lat);
        }
    return openrider::sdk::LocalProjection((west + east) / 2.0, (south + north) / 2.0);
}

// Inserts every track, pass after pass: a track is split at its
// turn-arounds so that a stretch done out and back matches itself.
// @return false when cancelled
bool InsertTracks(Network &network, const std::vector<PlaneTrack> &tracks, const MatchParams &params, double arm,
                  const std::function<bool()> &cancelled)
{
    for (size_t t = 0; t < tracks.size(); ++t) {
        if (cancelled && cancelled())
            return false;
        const PlaneTrack &track = tracks[t];
        if (track.size() < 2)
            continue;
        size_t start = 0;
        std::vector<size_t> cuts = TurnArounds(track, arm, std::max(25.0, params.tolerance_m), 40.0);
        cuts.push_back(track.size() - 1);
        for (size_t cut : cuts) {
            if (cut <= start)
                continue;
            InsertPass(network, track.Slice(start, cut), static_cast<int>(t), params);
            start = cut;
        }
    }
    return true;
}

} // namespace

// @impl 0f87-6acf-6ab9-65de
openrider::sdk::SegmentNetwork BuildNetwork(const std::vector<openrider::sdk::RecordedTrack> &tracks, const MatchParams &params,
                                  const std::function<bool()> &cancelled)
{
    if (tracks.empty())
        return {};
    const openrider::sdk::LocalProjection projection = CentredProjection(tracks);
    std::vector<PlaneTrack> lines;
    for (const openrider::sdk::RecordedTrack &track : tracks)
        lines.push_back(ToPlane(track, projection));

    // 1-3. The network grown pass after pass.
    Network network(std::max(10.0, params.tolerance_m));
    const double arm = std::max(80.0, 4.0 * params.tolerance_m);
    if (!InsertTracks(network, lines, params, arm, cancelled) || (cancelled && cancelled()))
        return {};

    // 4. The network is built: tracks are now matched back to it with twice
    // the tolerance, so that a stretch of drift does not drop a track off its
    // path. Segments are cut where a track turns back, then each track gives
    // its chain of segments.
    MatchParams final_params = params;
    final_params.tolerance_m = 2.0 * params.tolerance_m;
    CutAtTurnArounds(network, lines, final_params, params.tolerance_m);
    TrackChains chains = ChainTracks(network, lines, tracks, final_params);
    MergeCloseNodes(network, params.tolerance_m);
    FillChainGaps(network, chains);
    openrider::sdk::SegmentNetwork result = ToSegmentNetwork(network, std::move(chains), tracks, projection);

    // 5. Shared clean-up (removes the unused segments and nodes too).
    openrider::sdk::CleanupParams cleanup;
    cleanup.min_turn_m = arm;
    cleanup.max_loop_m = 150.0;
    cleanup.duplicate_distance_m = std::max(25.0, 1.5 * params.tolerance_m);
    openrider::sdk::CleanUpNetwork(result, cleanup);
    return result;
}

} // namespace matching
