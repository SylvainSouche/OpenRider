/// @file
/// Geographic coordinates used as plane coordinates (plate carree):
/// x = longitude, y = latitude, in degrees.
#pragma once

#include "reproj/projection.h"

namespace reproj {

/// The identity projection of a geographic coordinate system (EPSG:4326).
class Geographic : public Projection {
public:
    /// x = lon, y = lat; false beyond the poles.
    bool Forward(double lon, double lat, double &x, double &y) const override;
    /// lon = x, lat = y; false beyond the poles.
    bool Inverse(double x, double y, double &lon, double &lat) const override;
};

} // namespace reproj
