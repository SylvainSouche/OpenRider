/// @file
/// Geometry in a local metric plane (module-level utilities): points in
/// metres, distances, polyline lengths, points along a polyline.
#pragma once

#include <cstddef>
#include <vector>

namespace core {

/// A point of the local plane, metres east (x) and north (y).
struct XY {
    double x = 0.0;  ///< metres east
    double y = 0.0;  ///< metres north
};

/// Distance between two points (m).
double Dist(const XY &a, const XY &b);

/// Length of a polyline (m).
double Length(const std::vector<XY> &line);

/// Point of `line` at `distance` metres along it from point `from`
/// (negative: backwards), clamped to the line's ends.
XY AlongFrom(const std::vector<XY> &line, size_t from, double distance);

/// Distance from `q` to the polyline `line` (to its nearest segment).
double DistanceToLine(const XY &q, const std::vector<XY> &line);

/// `line` resampled to `samples` points evenly spaced along it (both ends
/// kept).
std::vector<XY> Resampled(const std::vector<XY> &line, size_t samples);

} // namespace core
