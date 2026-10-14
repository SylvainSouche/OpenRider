#include "heatmap/stages.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace heatmap {

void FindTraversals(Assembly &assembly)
{
    const std::vector<RecordedTrack> &tracks = assembly.tracks;
    const Analysis &analysis = assembly.analysis;
    const std::vector<std::vector<XY>> &lines = assembly.analysis.lines;
    const int margin = assembly.margin;
    std::vector<std::vector<Trav>> &travs = assembly.travs;
    std::vector<std::vector<int>> &cut_candidates = assembly.cut_candidates;
    // 1. Traversals: each shared piece as monotone runs along its line (a
    // turn-around starts a new run); own pieces kept as they are.
    for (size_t t = 0; t < tracks.size(); ++t) {
        for (const TrackPiece &pc : analysis.pieces[t]) {
            if (pc.segment < 0) {
                travs[t].push_back({-1, 0, 0, pc.first, pc.last});
                continue;
            }
            const std::vector<Match> &m = analysis.matches[t];
            size_t f0 = pc.first;
            while (f0 < pc.last && m[f0].line != pc.segment)
                ++f0;
            int start_v = m[f0].vertex, ext_v = start_v, dir = 0;
            size_t start_i = pc.first, ext_i = f0;
            const std::vector<size_t> &turns = analysis.turns[t];
            for (size_t k = f0 + 1; k <= pc.last; ++k) {
                if (m[k].line != pc.segment)
                    continue;  // a bridged excursion point
                const int v = m[k].vertex;
                if (std::binary_search(turns.begin(), turns.end(), k)) {
                    // A detected turn-around: the traversal ends there and
                    // the next starts back from it, whatever its length.
                    travs[t].push_back({pc.segment, start_v, v, start_i, k});
                    start_v = ext_v = v;
                    start_i = ext_i = k;
                    dir = 0;
                    continue;
                }
                if (dir == 0) {
                    if (std::abs(v - start_v) > margin / 2)
                        dir = v > start_v ? 1 : -1;
                    ext_v = v;
                    ext_i = k;
                } else if ((dir > 0 && v >= ext_v) || (dir < 0 && v <= ext_v)) {
                    ext_v = v;
                    ext_i = k;
                } else if (std::abs(v - ext_v) > margin) {  // turned around
                    travs[t].push_back({pc.segment, start_v, ext_v, start_i, ext_i});
                    start_v = ext_v;
                    start_i = ext_i;
                    dir = -dir;
                    ext_v = v;
                    ext_i = k;
                }
            }
            travs[t].push_back({pc.segment, start_v, ext_v, start_i, pc.last});
        }
        for (const Trav &tr : travs[t]) {
            if (tr.line < 0)
                continue;
            const int line_length = static_cast<int>(lines[static_cast<size_t>(tr.line)].size());
            for (int v : {tr.from_v, tr.to_v})
                if (v > margin && v < line_length - 1 - margin)
                    cut_candidates[static_cast<size_t>(tr.line)].push_back(v);
        }
    }
}

void CutSharedLines(Assembly &assembly)
{
    const Analysis &analysis = assembly.analysis;
    const std::vector<std::vector<XY>> &lines = assembly.analysis.lines;
    const int margin = assembly.margin;
    std::vector<std::vector<int>> &cut_candidates = assembly.cut_candidates;
    std::vector<std::vector<int>> &cuts = assembly.cuts;
    // 2. Cuts per shared line: line ends plus clustered entry/exit points.
    for (size_t li = 0; li < lines.size(); ++li) {
        if (!analysis.kept[li])
            continue;
        const int line_length = static_cast<int>(lines[li].size());
        std::vector<int> c = cut_candidates[li];
        std::sort(c.begin(), c.end());
        cuts[li].push_back(0);
        for (size_t i = 0; i < c.size();) {
            size_t j = i;
            long sum = c[i];
            while (j + 1 < c.size() && c[j + 1] - c[j] <= margin)
                sum += c[++j];
            const int rep = static_cast<int>(std::lround(static_cast<double>(sum) / static_cast<double>(j - i + 1)));
            if (rep - cuts[li].back() > margin && line_length - 1 - rep > margin)
                cuts[li].push_back(rep);
            i = j + 1;
        }
        cuts[li].push_back(line_length - 1);
    }
}

} // namespace heatmap
