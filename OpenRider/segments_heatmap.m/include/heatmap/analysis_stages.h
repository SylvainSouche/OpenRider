/// @file
/// The stages of the heat-map analysis, in order (module-level
/// utilities). See heatmap.h for the method.
#pragma once

#include "heatmap/analysis.h"
#include "heatmap/geometry.h"

#include <unordered_set>
#include <vector>

namespace heatmap {

/// 1. Local projection around the tracks' centre; tracks in metres; their
/// turn-arounds (hairpins dropped) splitting each into passes.
void ProjectTracks(Analysis &analysis, const std::vector<RecordedTrack> &tracks, const SegmentParams &params);

/// 2-3. Blurred heat map of the passes, each drawn as a line trace_width_m
/// wide; the corridor: cells dense enough and reached by enough passes.
std::unordered_set<Cell> Corridor(const Analysis &analysis, const SegmentParams &params);

/// 4. Corridor components thinned to a skeleton, traced junction to
/// junction into lines (short spurs pruned), densified one vertex per cell.
void TraceLines(Analysis &analysis, const std::unordered_set<Cell> &corridor, const SegmentParams &params);

/// 5. Each track point matched to the nearest skeleton vertex within the
/// snapping tolerance, where the track moves along the line.
void MapTracks(Analysis &analysis, const std::vector<RecordedTrack> &tracks, const SegmentParams &params);

/// Pieces: each track's runs on one line (long enough) and its own parts;
/// short excursions off a line and back are no excursion.
void FindPieces(Analysis &analysis, const std::vector<RecordedTrack> &tracks, const SegmentParams &params);

/// 6. Shared lines (followed by enough passes) and their mean tracks;
/// pieces on the other lines become their tracks' own parts.
void MeanLines(Analysis &analysis, const SegmentParams &params);

} // namespace heatmap
