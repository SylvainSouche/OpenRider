/// @file
/// Mercator (EPSG methods 9804/9805, and 1024 for Web Mercator): on an
/// ellipsoid, or on a sphere (flattening 0) for the Web Mercator of map
/// tiles.
#pragma once

#include "reproj/projection.h"

namespace reproj {

/// Latitude of the top and bottom edges of the Web Mercator world.
constexpr double kWebMercatorMaxLatitude = 85.0511287798066;

/// Normal-aspect Mercator, ellipsoidal or spherical.
class Mercator : public Projection {
public:
    /// Mercator on `ellipsoid` with scale k0 on the equator, central
    /// meridian lon0 (degrees), false easting and northing (metres).
    Mercator(Ellipsoid ellipsoid, double lon0, double k0, double false_easting, double false_northing);

    /// false at and beyond kMaxLatitude, where y grows without bound.
    bool Forward(double lon, double lat, double &x, double &y) const override;
    /// Always defined for finite (x, y).
    bool Inverse(double x, double y, double &lon, double &lat) const override;

    /// Highest latitude projected, degrees.
    static constexpr double kMaxLatitude = 89.5;

private:
    double a_;               ///< semi-major axis, metres
    double e_;               ///< eccentricity (0 for a sphere)
    double lon0_;            ///< central meridian, degrees
    double k0_;              ///< scale on the equator
    double false_easting_;   ///< metres
    double false_northing_;  ///< metres
};

} // namespace reproj
