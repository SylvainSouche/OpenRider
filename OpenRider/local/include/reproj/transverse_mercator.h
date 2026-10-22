/// @file
/// Ellipsoidal transverse Mercator (EPSG method 9807): UTM and national
/// grids. Krüger's series to the fourth order in n, as given by Karney
/// (2011), "Transverse Mercator with an accuracy of a few nanometers":
/// agrees with PROJ to a tenth of a millimetre up to 20 degrees from the
/// central meridian (projection_test).
#pragma once

#include "reproj/projection.h"

namespace reproj {

/// Transverse Mercator on an ellipsoid.
class TransverseMercator : public Projection {
public:
    /// Projection centred on (lon0, lat0) degrees, with scale k0 on the
    /// central meridian and the given false easting and northing (metres).
    TransverseMercator(Ellipsoid ellipsoid, double lat0, double lon0, double k0, double false_easting,
                       double false_northing);

    /// false more than kMaxSpread degrees of longitude from the central
    /// meridian, where the series no longer converges usefully.
    bool Forward(double lon, double lat, double &x, double &y) const override;
    /// false where (x, y) lies beyond the same spread.
    bool Inverse(double x, double y, double &lon, double &lat) const override;

    /// Largest longitude difference from the central meridian projected.
    static constexpr double kMaxSpread = 60.0;

private:
    double lon0_;            ///< central meridian, degrees
    double k0_;              ///< scale on the central meridian
    double false_easting_;   ///< metres
    double false_northing_;  ///< metres
    double e_;               ///< eccentricity
    double rectifying_;      ///< A: radius of the rectifying sphere, metres
    double origin_xi_;       ///< xi of lat0 on the central meridian
    double alpha_[4];        ///< forward series coefficients
    double beta_[4];         ///< inverse series coefficients
};

} // namespace reproj
