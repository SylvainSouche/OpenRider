/// @file
/// Plane geometry of the heat-map method (module-level utilities): the
/// local projection, grid cells, densifying and simplifying polylines.
#pragma once

#include <openrider/sdk/plane_geometry.h>

#include <cstdint>
#include <vector>

namespace heatmap {

using openrider::sdk::AlongFrom;
using openrider::sdk::Dist;
using openrider::sdk::Length;
using openrider::sdk::XY;

/// Pi.
inline constexpr double kPi = 3.14159265358979323846;

/// Local equirectangular projection, metres; fine at a regional scale.
struct Proj {
    double lat0 = 0;  ///< centre latitude, degrees
    double lon0 = 0;  ///< centre longitude, degrees
    double kx = 1;    ///< metres per degree east
    double ky = 1;    ///< metres per degree north

    /// WGS84 to metres.
    void Fwd(double lon, double lat, double &x, double &y) const
    {
        x = (lon - lon0) * kx;
        y = (lat - lat0) * ky;
    }

    /// Metres to WGS84.
    void Inv(double x, double y, double &lon, double &lat) const
    {
        lon = lon0 + x / kx;
        lat = lat0 + y / ky;
    }
};

/// A grid cell, its two integer coordinates packed.
using Cell = int64_t;

/// The cell (ix, iy).
inline Cell Key(int64_t ix, int64_t iy) { return (ix << 32) ^ (iy & 0xffffffffLL); }

/// Column of a cell.
inline int64_t KeyX(Cell c) { return c >> 32; }

/// Row of a cell.
inline int64_t KeyY(Cell c) { return static_cast<int32_t>(c & 0xffffffffLL); }

/// Points every `step` metres along a polyline (ends included).
std::vector<XY> Densify(const std::vector<XY> &line, double step);

/// Douglas-Peucker simplification: points within `epsilon` of the
/// simplified line dropped (ends kept).
std::vector<XY> Simplify(const std::vector<XY> &line, double epsilon);

} // namespace heatmap
