/// @file
/// Lambert conformal conic (EPSG methods 9801 and 9802): Lambert-93,
/// Belgian Lambert, Austria Lambert and other national grids. Closed
/// formulas of EPSG guidance note 7-2; exact to rounding.
#pragma once

#include "reproj/projection.h"

namespace reproj {

/// Lambert conformal conic on an ellipsoid, one or two standard parallels.
class LambertConformalConic : public Projection {
public:
    /// Cone through the parallels lat1 and lat2 (degrees; equal for a
    /// single standard parallel, then scaled by k0), origin at (lon0,
    /// lat0), with the given false easting and northing (metres).
    LambertConformalConic(Ellipsoid ellipsoid, double lat0, double lon0, double lat1, double lat2, double k0,
                          double false_easting, double false_northing);

    /// false at the pole opposite the cone's apex, and beyond it.
    bool Forward(double lon, double lat, double &x, double &y) const override;
    /// Defined everywhere but at the apex.
    bool Inverse(double x, double y, double &lon, double &lat) const override;

private:
    /// EPSG's t of a latitude in radians.
    double T(double phi) const;

    double a_;               ///< semi-major axis, metres
    double e_;               ///< eccentricity
    double lon0_;            ///< central meridian, degrees
    double false_easting_;   ///< metres
    double false_northing_;  ///< metres
    double n_;               ///< cone constant
    double f_;               ///< EPSG's F, times k0 and the semi-major axis
    double r0_;              ///< radius of the origin's parallel, metres
};

} // namespace reproj
