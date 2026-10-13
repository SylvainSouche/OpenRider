#include "heatmap/heatmap.h"

#include "heatmap/analysis.h"

#include <algorithm>
#include <map>

namespace heatmap {

// @impl 0f87-6acd-6028-c636
SegmentResult FindSegments(const std::vector<OverviewTrack> &tracks, const SegmentParams &params)
{
    SegmentResult out;
    if (tracks.empty())
        return out;
    const Analysis analysis = Analyse(tracks, params);
    std::vector<int> new_id(analysis.lines.size(), -1);
    for (size_t li = 0; li < analysis.lines.size(); ++li) {
        if (!analysis.kept[li])
            continue;
        Segment sg;
        sg.id = static_cast<int>(out.segments.size());
        const std::vector<XY> mean = Simplify(analysis.mean[li], params.cell_m * 0.3);
        sg.length_m = Length(mean);
        for (const XY &q : mean) {
            double lon, lat;
            analysis.pr.Inv(q.x, q.y, lon, lat);
            sg.lon.push_back(lon);
            sg.lat.push_back(lat);
        }
        for (size_t t : analysis.line_tracks[li])
            sg.track_ids.push_back(tracks[t].id);
        new_id[li] = sg.id;
        out.segments.push_back(std::move(sg));
    }
    for (size_t t = 0; t < tracks.size(); ++t) {
        TrackMapping tm;
        tm.id = tracks[t].id;
        for (TrackPiece pc : analysis.pieces[t]) {
            pc.segment = pc.segment >= 0 ? new_id[static_cast<size_t>(pc.segment)] : -1;
            tm.pieces.push_back(pc);
        }
        out.mappings.push_back(std::move(tm));
    }
    return out;
}

} // namespace heatmap
