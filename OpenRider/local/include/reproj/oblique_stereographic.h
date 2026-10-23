/// @file
/// Oblique stereographic (EPSG method 9809, PROJ "sterea"): the Dutch RD
/// grid and Romania's Stereo 70. The ellipsoid is mapped conformally onto
/// a sphere (Gauss), then projected stereographically from it.
#pragma once

#include "reproj/projection.h"

namespace reproj {

/// Oblique stereographic, double projection through the conformal sphere.
class ObliqueStereographic : public Projection {
public:
    /// Centred on (lon0, lat0) degrees with scale k0 there and the given
    /// false easting and northing (metres).
    ObliqueStereographic(Ellipsoid ellipsoid, double lat0, double lon0, double k0, double false_easting,
                         double false_northing);

    /// false at the antipode of the centre.
    bool Forward(double lon, double lat, double &x, double &y) const override;
    /// Defined everywhere.
    bool Inverse(double x, double y, double &lon, double &lat) const override;

private:
    double e_;               ///< eccentricity
    double lon0_;            ///< longitude of the centre, degrees
    double false_easting_;   ///< metres
    double false_northing_;  ///< metres
    double two_r_k0_;        ///< 2 R k0: the sphere's diameter, scaled
    double n_;               ///< longitude ratio sphere / ellipsoid
    double c_;               ///< latitude constant of the conformal sphere
    double chi0_;            ///< conformal latitude of the centre, radians
};

} // namespace reproj
