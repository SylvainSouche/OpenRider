#include "matching/matching.h"

#include "matching/insertion.h"
#include "matching/network.h"
#include "matching/output.h"
#include "matching/routes.h"
#include "matching/track_geometry.h"

#include <openrider/sdk/local_projection.h>
#include <openrider/sdk/network_cleanup.h>

#include <algorithm>

namespace matching {

namespace {

using openrider::sdk::XY;

// Projection around the tracks' centre.
openrider::sdk::LocalProjection CentredProjection(const std::vector<openrider::sdk::OverviewTrack> &tracks)
{
    double west = 1e9, east = -1e9, south = 1e9, north = -1e9;
    for (const openrider::sdk::OverviewTrack &track : tracks)
        for (size_t i = 0; i < track.lat.size(); ++i) {
            west = std::min(west, track.lon[i]);
            east = std::max(east, track.lon[i]);
            south = std::min(south, track.lat[i]);
            north = std::max(north, track.lat[i]);
        }
    return openrider::sdk::LocalProjection((west + east) / 2.0, (south + north) / 2.0);
}

// Each track in the plane, resampled every station_m.
std::vector<std::vector<XY>> ProjectTracks(const std::vector<openrider::sdk::OverviewTrack> &tracks,
                                           const openrider::sdk::LocalProjection &projection, const MatchParams &params)
{
    std::vector<std::vector<XY>> lines;
    for (const openrider::sdk::OverviewTrack &track : tracks) {
        std::vector<XY> line;
        for (size_t i = 0; i < track.lat.size(); ++i)
            line.push_back(projection.Forward(track.lon[i], track.lat[i]));
        lines.push_back(line.size() >= 2 ? Stations(line, params.station_m) : line);
    }
    return lines;
}

// Inserts every track, pass after pass: a track is split at its
// turn-arounds so that a stretch done out and back matches itself.
// @return false when cancelled
bool InsertTracks(Network &network, const std::vector<std::vector<XY>> &lines, const MatchParams &params, double arm,
                  const std::function<bool()> &cancelled)
{
    for (size_t t = 0; t < lines.size(); ++t) {
        if (cancelled && cancelled())
            return false;
        const std::vector<XY> &line = lines[t];
        if (line.size() < 2)
            continue;
        size_t start = 0;
        std::vector<size_t> cuts = TurnArounds(line, params.station_m, arm, std::max(25.0, params.tolerance_m), 40.0);
        cuts.push_back(line.size() - 1);
        for (size_t cut : cuts) {
            if (cut <= start)
                continue;
            const std::vector<XY> pass(line.begin() + static_cast<long>(start), line.begin() + static_cast<long>(cut) + 1);
            InsertPass(network, pass, static_cast<int>(t), params);
            start = cut;
        }
    }
    return true;
}

} // namespace

// @impl 0f87-6ace-2f7b-f4d7
openrider::sdk::SegmentNetwork BuildNetwork(const std::vector<openrider::sdk::OverviewTrack> &tracks, const MatchParams &params,
                                  const std::function<bool()> &cancelled)
{
    if (tracks.empty())
        return {};
    const openrider::sdk::LocalProjection projection = CentredProjection(tracks);
    const std::vector<std::vector<XY>> lines = ProjectTracks(tracks, projection, params);

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
