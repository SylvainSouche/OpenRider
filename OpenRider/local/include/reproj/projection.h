/// @file
/// A map projection: geographic coordinates (degrees) to projected
/// coordinates and back. The base of every coordinate system's projection.
#pragma once

namespace reproj {

/// An ellipsoid: semi-major axis (metres) and flattening.
struct Ellipsoid {
    double a;  ///< semi-major axis, metres
    double f;  ///< flattening
};

/// GRS 80, the ellipsoid of ETRS89 and RGF93.
constexpr Ellipsoid kGrs80{6378137.0, 1.0 / 298.257222101};
/// WGS 84.
constexpr Ellipsoid kWgs84{6378137.0, 1.0 / 298.257223563};

/// Converts geographic coordinates (longitude, latitude in degrees) to
/// projected ones (x east, y north) and back. Stateless after construction:
/// safe to share between threads.
class Projection {
public:
    virtual ~Projection() = default;

    /// Projects (lon, lat) degrees to (x, y).
    /// @return false where the projection is undefined or unreliable
    virtual bool Forward(double lon, double lat, double &x, double &y) const = 0;

    /// Unprojects (x, y) to (lon, lat) degrees.
    /// @return false where (x, y) maps to no point of the ellipsoid
    virtual bool Inverse(double x, double y, double &lon, double &lat) const = 0;
};

} // namespace reproj
