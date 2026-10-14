/// @file
/// Geometry of tracks and lines in the local plane, for the matching
/// (module-level utilities).
#pragma once

#include "matching/plane_track.h"

#include <openrider/sdk/plane_geometry.h>

#include <cstddef>
#include <vector>

namespace matching {

/// `line` resampled every `step` metres (at least its two ends).
std::vector<openrider::sdk::XY> Stations(const std::vector<openrider::sdk::XY> &line, double step);

/// Each point of `line` replaced by the mean of the points within `half`
/// stations of it (fewer near the ends): neighbouring stations are means of
/// different subsets of the tracks and jitter across the path.
std::vector<openrider::sdk::XY> Smoothed(const std::vector<openrider::sdk::XY> &line, int half);

/// Indices where `track` turns back on itself: `arm` metres of path before
/// and after, it is within `close` of itself while the point stands out
/// from both by more than `spike`. One index per turn.
std::vector<size_t> TurnArounds(const PlaneTrack &track, double arm, double close, double spike);

} // namespace matching
