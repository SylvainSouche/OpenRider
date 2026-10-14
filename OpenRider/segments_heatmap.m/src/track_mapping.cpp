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

void MapTracks(Analysis &analysis, const std::vector<RecordedTrack> &tracks, const SegmentParams &params)
{
    const std::vector<std::vector<XY>> &xy = analysis.xy;
    std::vector<std::vector<XY>> &lines = analysis.lines;
    // 5. Map tracks: nearest skeleton vertex within tolerance (grid index).
    // Points within half the trace width of a skeleton belong to it.
    const double r = std::max(params.tolerance_m, std::max(0.0, params.trace_width_m / 2.0));
    const double bucket = std::max(25.0, r * 2.0);  // >= snap: neighbours of the bucket hold every candidate
    std::unordered_map<Cell, std::vector<std::pair<int, int>>> index;
    for (int li = 0; li < static_cast<int>(lines.size()); ++li)
        for (int vi = 0; vi < static_cast<int>(lines[static_cast<size_t>(li)].size()); ++vi) {
            const XY &v = lines[static_cast<size_t>(li)][static_cast<size_t>(vi)];
            index[Key(static_cast<int64_t>(std::floor(v.x / bucket)), static_cast<int64_t>(std::floor(v.y / bucket)))]
                .push_back({li, vi});
        }
    const double snap = std::max(r * 1.5, 25.0);  // GPS drift on a shared path (forest: 15-25 m)
    std::vector<std::vector<Match>> &matches = analysis.matches;
    matches.assign(tracks.size(), {});
    // A point matches a line only where the track moves along it (either
    // way): its own velocity within 60 degrees of the line's direction.
    // Position alone puts a hairpin's tip on a leg, or a point on the wrong
    // one of two close parallel paths; direction tells them apart. (Points
    // not moving are not analysed at all.)
    auto line_direction = [&](int li, int vi) {
        const std::vector<XY> &l = lines[static_cast<size_t>(li)];
        const XY &a = l[static_cast<size_t>(std::max(0, vi - 2))];
        const XY &b = l[std::min(l.size() - 1, static_cast<size_t>(vi + 2))];
        return XY{b.x - a.x, b.y - a.y};
    };
    for (size_t t = 0; t < tracks.size(); ++t) {
        for (size_t qi = 0; qi < xy[t].size(); ++qi) {
            const XY &q = xy[t][qi];
            const XY &velocity = analysis.velocity[t][qi];
            const double speed = std::hypot(velocity.x, velocity.y);
            Match m;
            double best = snap;
            const int64_t bx = static_cast<int64_t>(std::floor(q.x / bucket)), by = static_cast<int64_t>(std::floor(q.y / bucket));
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    auto it = index.find(Key(bx + dx, by + dy));
                    if (it == index.end())
                        continue;
                    for (auto [li, vi] : it->second) {
                        const XY &v = lines[static_cast<size_t>(li)][static_cast<size_t>(vi)];
                        const double d = Dist(v, q);
                        if (d >= best)
                            continue;
                        if (speed > 0.0) {
                            const XY along = line_direction(li, vi);
                            const double length = std::hypot(along.x, along.y);
                            if (length > 0.0 &&
                                std::fabs(velocity.x * along.x + velocity.y * along.y) < 0.5 * speed * length)
                                continue;  // moving across the line, not along it
                        }
                        {
                            best = d;
                            m.line = li;
                            m.vertex = vi;
                        }
                    }
                }
            if (m.line >= 0) {
                const std::vector<XY> &l = lines[static_cast<size_t>(m.line)];
                const size_t a = static_cast<size_t>(std::max(0, m.vertex - 1));
                const size_t b = std::min(l.size() - 1, static_cast<size_t>(m.vertex + 1));
                const double tx = l[b].x - l[a].x, ty = l[b].y - l[a].y, tl = std::hypot(tx, ty);
                const XY &v = l[static_cast<size_t>(m.vertex)];
                m.offset = tl > 0 ? ((q.x - v.x) * -ty + (q.y - v.y) * tx) / tl : 0.0;
            }
            matches[t].push_back(m);
        }
    }
}

void FindPieces(Analysis &analysis, const std::vector<RecordedTrack> &tracks, const SegmentParams &params)
{
    const std::vector<std::vector<XY>> &xy = analysis.xy;
    std::vector<std::vector<XY>> &lines = analysis.lines;
    std::vector<std::vector<Match>> &matches = analysis.matches;
    // Pieces: runs on one line, long enough; the rest is the track's own.
    std::vector<std::set<size_t>> &line_tracks = analysis.line_tracks;
    line_tracks.assign(lines.size(), {});
    std::vector<std::set<std::pair<size_t, int>>> &line_passes = analysis.line_passes;
    line_passes.assign(lines.size(), {});
    std::vector<std::vector<TrackPiece>> &pieces = analysis.pieces;
    pieces.assign(tracks.size(), {});
    for (size_t t = 0; t < tracks.size(); ++t) {
        const std::vector<Match> &ms = matches[t];
        size_t i = 0;
        while (i < ms.size()) {
            size_t j = i;
            while (j + 1 < ms.size() && ms[j + 1].line == ms[i].line)
                ++j;
            TrackPiece pc{ms[i].line, i, j};
            double len = 0.0;
            for (size_t k2 = i + 1; k2 <= j; ++k2)
                len += Dist(xy[t][k2 - 1], xy[t][k2]);
            if (pc.segment >= 0 && len < params.min_piece_m)
                pc.segment = -1;
            if (!pieces[t].empty() && pieces[t].back().segment == pc.segment)
                pieces[t].back().last = j;
            else
                pieces[t].push_back(pc);
            if (pc.segment >= 0) {
                line_tracks[static_cast<size_t>(pc.segment)].insert(t);
                for (size_t k = i; k <= j; ++k)
                    line_passes[static_cast<size_t>(pc.segment)].insert({t, analysis.pass_of[t][k]});
            }
            i = j + 1;
        }
    }

    // Short excursions: a track leaving a line and rejoining the same line
    // within max_excursion_m (GPS drift, a passing place) never left it.
    for (size_t t = 0; t < pieces.size(); ++t) {
        std::vector<TrackPiece> &ps = pieces[t];
        std::vector<TrackPiece> merged;
        for (size_t i = 0; i < ps.size(); ++i) {
            if (ps[i].segment < 0 && !merged.empty() && i + 1 < ps.size() &&
                merged.back().segment >= 0 && ps[i + 1].segment == merged.back().segment) {
                double len = 0.0;
                for (size_t k = ps[i].first; k <= ps[i].last + 1 && k < xy[t].size(); ++k)
                    if (k > 0)
                        len += Dist(xy[t][k - 1], xy[t][k]);
                if (len <= params.max_excursion_m) {
                    merged.back().last = ps[i + 1].last;
                    ++i;  // the rejoined piece is absorbed too
                    continue;
                }
            }
            merged.push_back(ps[i]);
        }
        ps.swap(merged);
    }
}

void MeanLines(Analysis &analysis, const SegmentParams &params)
{
    std::vector<std::vector<XY>> &lines = analysis.lines;
    std::vector<std::vector<Match>> &matches = analysis.matches;
    std::vector<std::vector<TrackPiece>> &pieces = analysis.pieces;
    std::vector<std::set<size_t>> &line_tracks = analysis.line_tracks;
    std::vector<std::set<std::pair<size_t, int>>> &line_passes = analysis.line_passes;
    // Shared lines (>= min_tracks routes) and their mean tracks; pieces on
    // other lines become their tracks' own parts.
    analysis.kept.assign(lines.size(), false);
    analysis.mean.assign(lines.size(), {});
    for (size_t li = 0; li < lines.size(); ++li) {
        // Shared: followed by enough passes (an out-and-back counts twice).
        if (static_cast<int>(line_passes[li].size()) < params.min_tracks)
            continue;
        analysis.kept[li] = true;
        const std::vector<XY> &l = lines[li];
        // Offset per vertex: mean over tracks of each track's mean offset.
        std::vector<std::map<size_t, std::pair<double, int>>> per(l.size());
        for (size_t t : line_tracks[li])
            for (const TrackPiece &pc : pieces[t])
                if (pc.segment == static_cast<int>(li))
                    for (size_t k2 = pc.first; k2 <= pc.last; ++k2) {
                        const Match &m = matches[t][k2];
                        if (m.line != static_cast<int>(li))
                            continue;  // a bridged excursion point
                        auto &acc = per[static_cast<size_t>(m.vertex)][t];
                        acc.first += m.offset;
                        ++acc.second;
                    }
        std::vector<double> off(l.size(), std::numeric_limits<double>::quiet_NaN());
        for (size_t v = 0; v < l.size(); ++v) {
            double sum = 0.0;
            int nt = 0;
            for (const auto &[t, acc] : per[v]) {
                sum += acc.first / acc.second;
                ++nt;
            }
            if (nt > 0)
                off[v] = sum / nt;
        }
        // Smooth and fill gaps over +/- 4 vertices.
        for (size_t v = 0; v < l.size(); ++v) {
            double sum = 0.0;
            int nn = 0;
            for (size_t u = v >= 4 ? v - 4 : 0; u <= std::min(l.size() - 1, v + 4); ++u)
                if (!std::isnan(off[u])) {
                    sum += off[u];
                    ++nn;
                }
            const double o = nn ? sum / nn : 0.0;
            const size_t a = v ? v - 1 : 0, b = std::min(l.size() - 1, v + 1);
            const double tx = l[b].x - l[a].x, ty = l[b].y - l[a].y, tl = std::hypot(tx, ty);
            analysis.mean[li].push_back(tl > 0 ? XY{l[v].x - ty / tl * o, l[v].y + tx / tl * o} : l[v]);
        }
    }
    for (std::vector<TrackPiece> &ps : pieces) {
        std::vector<TrackPiece> merged;
        for (TrackPiece pc : ps) {
            if (pc.segment >= 0 && !analysis.kept[static_cast<size_t>(pc.segment)])
                pc.segment = -1;
            if (!merged.empty() && merged.back().segment == pc.segment)
                merged.back().last = pc.last;
            else
                merged.push_back(pc);
        }
        ps.swap(merged);
    }
    for (size_t li = 0; li < lines.size(); ++li)
        if (!analysis.kept[li])
            line_tracks[li].clear();
}

} // namespace heatmap
