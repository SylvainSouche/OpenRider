/// @file
/// Lambert azimuthal equal area (EPSG method 9820, PROJ "laea"), the
/// ellipsoidal oblique aspect: LAEA Europe (EPSG:3035).
#pragma once

#include "reproj/projection.h"

namespace reproj {

/// Lambert azimuthal equal area on an ellipsoid, through the authalic
/// sphere.
class LambertAzimuthalEqualArea : public Projection {
public:
    /// Centred on (lon0, lat0) degrees, with the given false easting and
    /// northing (metres).
    LambertAzimuthalEqualArea(Ellipsoid ellipsoid, double lat0, double lon0, double false_easting,
                              double false_northing);

    /// false at the antipode of the centre.
    bool Forward(double lon, double lat, double &x, double &y) const override;
    /// false beyond the antipode's circle.
    bool Inverse(double x, double y, double &lon, double &lat) const override;

private:
    /// Authalic q of a latitude in radians.
    double Q(double phi) const;

    double e_;               ///< eccentricity
    double lon0_;            ///< longitude of the centre, degrees
    double false_easting_;   ///< metres
    double false_northing_;  ///< metres
    double q_pole_;          ///< q at the pole
    double rq_;              ///< radius of the authalic sphere, metres
    double beta0_;           ///< authalic latitude of the centre, radians
    double d_;               ///< EPSG's D: scale correction along the meridian
};

} // namespace reproj
