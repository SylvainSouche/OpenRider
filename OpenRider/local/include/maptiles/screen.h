// Screen-space simplification of a polyline before drawing: points closer
// than min_px to the last kept one are dropped (first and last always
// kept). Many GPS points can fall within one pixel when zoomed out; drawn
// as a thick line, their sub-pixel zigzags would turn into miter spikes.
#pragma once

#include <cstddef>
#include <vector>

namespace maptiles {

constexpr float kMinSegmentPx = 1.5f;

// Indices of the points to draw.
std::vector<size_t> simplify_screen(const std::vector<float> &xs, const std::vector<float> &ys,
                                    float min_px);

} // namespace maptiles
