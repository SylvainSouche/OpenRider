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

void ProjectTracks(Analysis &analysis, const std::vector<OverviewTrack> &tracks, const SegmentParams &params)
{
    // 1. Projection around the tracks' centre.
    Proj &pr = analysis.pr;
    double w = 1e9, e = -1e9, s = 1e9, n = -1e9;
    for (const OverviewTrack &t : tracks) {
        w = std::min(w, t.west);
        e = std::max(e, t.east);
        s = std::min(s, t.south);
        n = std::max(n, t.north);
    }
    pr.lat0 = (s + n) / 2.0;
    pr.lon0 = (w + e) / 2.0;
    pr.kx = 111320.0 * std::cos(pr.lat0 * kPi / 180.0);
    pr.ky = 110540.0;
    std::vector<std::vector<XY>> &xy = analysis.xy;
    xy.assign(tracks.size(), {});
    for (size_t t = 0; t < tracks.size(); ++t)
        for (size_t i = 0; i < tracks[t].lat.size(); ++i) {
            XY q;
            pr.Fwd(tracks[t].lon[i], tracks[t].lat[i], q.x, q.y);
            xy[t].push_back(q);
        }

    // Turn-arounds split each track into passes: a stretch done out and back
    // is passed twice, and the two passes count as two contributors to the
    // heat map, so that it forms a corridor and a segment like a stretch
    // shared by two outings. Scales follow the trace width.
    analysis.turns.assign(tracks.size(), {});
    analysis.pass_of.assign(tracks.size(), {});
    const double turn_arm = std::max(80.0, 2.0 * params.trace_width_m);
    for (size_t t = 0; t < tracks.size(); ++t)
        analysis.turns[t] = TurnArounds(xy[t], turn_arm, std::max(25.0, params.trace_width_m / 2.0),
                                        std::max(40.0, params.trace_width_m));
    DropHairpins(xy, turn_arm, analysis.turns);
    for (size_t t = 0; t < tracks.size(); ++t) {
        std::vector<int> &pass = analysis.pass_of[t];
        pass.assign(xy[t].size(), 0);
        size_t next_turn = 0;
        int current = 0;
        for (size_t i = 0; i < xy[t].size(); ++i) {
            pass[i] = current;
            if (next_turn < analysis.turns[t].size() && analysis.turns[t][next_turn] == i) {
                ++current;  // the turn point ends one pass and starts the next
                ++next_turn;
            }
        }
    }
}

Analysis Analyse(const std::vector<OverviewTrack> &tracks, const SegmentParams &params)
{
    Analysis analysis;
    ProjectTracks(analysis, tracks, params);
    TraceLines(analysis, Corridor(analysis, params), params);
    MapTracks(analysis, tracks, params);
    FindPieces(analysis, tracks, params);
    MeanLines(analysis, params);
    return analysis;
}

} // namespace heatmap
