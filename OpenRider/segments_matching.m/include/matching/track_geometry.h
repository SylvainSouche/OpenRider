/// @file
/// Geometry of tracks and lines in the local plane, for the matching
/// (module-level utilities).
#pragma once

#include <openrider/sdk/plane_geometry.h>

#include <cstddef>
#include <vector>

namespace matching {

/// `line` resampled every `step` metres (at least its two ends).
std::vector<openrider::sdk::XY> Stations(const std::vector<openrider::sdk::XY> &line, double step);

/// Directions of travel along `line`: from the previous point to the next
/// (not normalised).
std::vector<openrider::sdk::XY> Headings(const std::vector<openrider::sdk::XY> &line);

/// Each point of `line` replaced by the mean of the points within `half`
/// stations of it (fewer near the ends): neighbouring stations are means of
/// different subsets of the tracks and jitter across the path.
std::vector<openrider::sdk::XY> Smoothed(const std::vector<openrider::sdk::XY> &line, int half);

/// Indices where `line` (evenly spaced every `step`) turns back on itself:
/// `arm` metres before and after, it is within `close` of itself while the
/// point stands out from both by more than `spike`. One index per turn.
std::vector<size_t> TurnArounds(const std::vector<openrider::sdk::XY> &line, double step, double arm, double close, double spike);

} // namespace matching
