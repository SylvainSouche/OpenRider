/// @file
/// Screen-space simplification of a polyline before drawing (module-level
/// utilities).
#pragma once

#include <cstddef>
#include <vector>

namespace maptiles {

/// Shortest segment drawn, in pixels.
constexpr float kMinSegmentPx = 1.5f;

/// Points of a screen polyline worth drawing: points closer than `min_px`
/// to the last kept one are dropped (first and last always kept). Many GPS
/// points can fall within one pixel when zoomed out; drawn as a thick line,
/// their sub-pixel zigzags would turn into miter spikes.
/// @return indices of the points to draw
std::vector<size_t> SimplifyScreen(const std::vector<float> &xs, const std::vector<float> &ys,
                                   float min_px);

} // namespace maptiles
