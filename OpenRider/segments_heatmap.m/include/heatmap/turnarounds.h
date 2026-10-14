/// @file
/// Turn-arounds of tracks, told apart from hairpin bends (module-level
/// utilities).
#pragma once

#include "heatmap/geometry.h"

#include <cstddef>
#include <vector>

namespace heatmap {

/// Indices of the points where a track turns back on itself: the track a
/// little before and a little after (`arm` metres of path, `along` giving
/// each point's) passes within `close` of itself while the point stands out
/// from both by more than `spike` -- an out-and-back. One index per turn:
/// the farthest point.
std::vector<size_t> TurnArounds(const std::vector<XY> &line, const std::vector<double> &along, double arm, double close,
                                double spike);

/// Removes the turns made at hairpin bends: where at least three tracks
/// pass a turn place and nearly all of them double back there, it is a
/// bend of the path, not a turn-around.
void DropHairpins(const std::vector<std::vector<XY>> &xy, const std::vector<std::vector<double>> &along, double arm,
                  std::vector<std::vector<size_t>> &turns);

} // namespace heatmap
