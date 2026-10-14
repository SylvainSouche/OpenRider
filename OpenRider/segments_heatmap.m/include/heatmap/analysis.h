/// @file
/// The analysis both outputs of the heat-map method start from.
#pragma once

#include "heatmap/geometry.h"
#include "heatmap/heatmap.h"

#include <set>
#include <utility>
#include <vector>

namespace heatmap {

/// Where a track point lies on the skeleton lines.
struct Match {
    int line = -1;        ///< skeleton line, -1 = none within tolerance
    int vertex = -1;      ///< nearest vertex on it
    double offset = 0.0;  ///< signed distance from the skeleton
};

/// Everything both outputs need: projection, tracks in metres, their
/// turn-arounds and passes, skeleton lines (densified), per-point matches,
/// pieces, and the mean track of each kept (shared) line, one point per
/// skeleton vertex.
struct Analysis {
    Proj pr;                                    ///< local projection
    std::vector<std::vector<XY>> xy;            ///< tracks' moving recorded points, metres
    std::vector<std::vector<XY>> velocity;      ///< per point: its velocity (m/s)
    std::vector<std::vector<double>> along;     ///< per point: path from the track's first point (m)
    std::vector<std::vector<size_t>> turns;     ///< per track: turn-around (and rest) point indices, ending passes
    std::vector<openrider::sdk::RestPlace> rest_places;  ///< where tracks rest
    std::vector<XY> rest_points;                ///< the same, metres
    std::vector<std::vector<int>> pass_of;      ///< per track point: pass index (turn-arounds split passes)
    std::vector<std::vector<XY>> lines;         ///< skeleton lines, one vertex per cell
    std::vector<std::vector<Match>> matches;    ///< per track point
    std::vector<std::vector<TrackPiece>> pieces;  ///< per track: runs on lines and its own parts
    std::vector<std::set<size_t>> line_tracks;    ///< tracks along each line
    std::vector<std::set<std::pair<size_t, int>>> line_passes;  ///< (track, pass) along each line
    std::vector<bool> kept;                     ///< shared lines
    std::vector<std::vector<XY>> mean;          ///< per kept line, per vertex
};

/// Runs the heat-map analysis of `tracks` (see heatmap.h, steps 1-6).
Analysis Analyse(const std::vector<RecordedTrack> &tracks, const SegmentParams &params);

} // namespace heatmap
