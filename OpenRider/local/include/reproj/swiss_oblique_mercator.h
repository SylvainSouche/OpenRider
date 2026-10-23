/// @file
/// Swiss oblique Mercator (EPSG method 9815 variant, PROJ "somerc"): the
/// Swiss LV95 grid and Hungary's EOV. Rigorous formulas of swisstopo's
/// "Formulas and constants for the calculation of the Swiss conformal
/// cylindrical projection": the ellipsoid onto a conformal sphere, then an
/// oblique Mercator of the sphere.
#pragma once

#include "reproj/projection.h"

namespace reproj {

/// Oblique Mercator through the conformal sphere, centred on the origin.
class SwissObliqueMercator : public Projection {
public:
    /// Centred on (lon0, lat0) degrees with scale k0 there and the given
    /// false easting and northing (metres).
    SwissObliqueMercator(Ellipsoid ellipsoid, double lat0, double lon0, double k0, double false_easting,
                         double false_northing);

    /// false at the poles of the oblique cylinder.
    bool Forward(double lon, double lat, double &x, double &y) const override;
    /// Defined everywhere.
    bool Inverse(double x, double y, double &lon, double &lat) const override;

private:
    double e_;               ///< eccentricity
    double lon0_;            ///< longitude of the origin, degrees
    double false_easting_;   ///< metres
    double false_northing_;  ///< metres
    double alpha_;           ///< longitude ratio sphere / ellipsoid
    double b0_;              ///< latitude of the origin on the sphere, radians
    double k_;               ///< latitude constant of the conformal sphere
    double r_k0_;            ///< the sphere's radius, scaled by k0
};

} // namespace reproj
