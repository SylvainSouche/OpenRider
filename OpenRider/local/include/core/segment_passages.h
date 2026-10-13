/// @file
/// Where an outing travels a segment (module-level utilities): the
/// stretches of its recorded points along the segment's line, found from
/// the line and the points alone, whatever segments plugin made the
/// network.
#pragma once

#include "core/plane_geometry.h"
#include "core/segments.h"

#include <openrider/input.h>

#include <vector>

namespace core {

/// Where a point lies relative to a line.
struct LinePosition {
    double along_m = 0.0;     ///< distance along the line to the point's projection
    double distance_m = 0.0;  ///< distance from the line
};

/// Position of `q` relative to the polyline `line` (its nearest piece).
LinePosition PositionOnLine(const XY &q, const std::vector<XY> &line);

/// One passage of an outing along a segment.
struct SegmentPassage {
    size_t first = 0;              ///< first point index in the outing's points
    size_t last = 0;               ///< last point index, inclusive
    bool forward = true;           ///< travelled from the segment's start to its end
    std::vector<double> along_m;   ///< position along the segment of each point first..last
};

/// The passages of `points` along `segment`: stretches of points within
/// `max_distance_m` of its line (short excursions beyond bridged) that
/// cover at least `min_coverage` (0-1) of its length in one direction. A
/// stretch going along and back is two passages; so is an outing passing
/// the segment twice.
std::vector<SegmentPassage> FindPassages(const std::vector<or_point> &points, const NetworkSegment &segment,
                                         double max_distance_m, double min_coverage);

} // namespace core
