/// @file
/// Křovák (EPSG methods 9819 and 1041, PROJ "krovak"): the S-JTSK grid of
/// Czechia and Slovakia, an oblique conic on a conformal sphere. Output
/// as PROJ does by default: easting and northing, both negative there
/// (EPSG:5514, "Krovak East North").
#pragma once

#include "reproj/projection.h"

namespace reproj {

/// Oblique conformal conic of Křovák.
class Krovak : public Projection {
public:
    /// Projection centred at lat0 (degrees) on the meridian lon0, the
    /// cone's axis at co-latitude alpha (degrees), pseudo standard parallel
    /// lat_ts (degrees) with scale k0, false easting and northing (metres).
    Krovak(Ellipsoid ellipsoid, double lat0, double lon0, double alpha, double lat_ts, double k0,
           double false_easting, double false_northing);

    /// false at the cone's apex and beyond the oblique pole.
    bool Forward(double lon, double lat, double &x, double &y) const override;
    /// Defined everywhere but at the apex.
    bool Inverse(double x, double y, double &lon, double &lat) const override;

private:
    double e_;               ///< eccentricity
    double lon0_;            ///< longitude of the origin, degrees
    double false_easting_;   ///< metres
    double false_northing_;  ///< metres
    double alpha_c_;         ///< co-latitude of the cone's axis, radians
    double lat_p_;           ///< pseudo standard parallel, radians
    double b_;               ///< longitude ratio sphere / ellipsoid
    double t0_;              ///< latitude constant of the conformal sphere
    double n_;               ///< cone constant
    double r0_;              ///< radius of the pseudo standard parallel, metres
};

} // namespace reproj
