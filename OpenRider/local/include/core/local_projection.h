/// @file
/// Local metric projection of WGS84 positions.
#pragma once

#include "core/plane_geometry.h"

namespace core {

/// Equirectangular projection around a centre: metres east and north of
/// it. Accurate to a fraction of a percent over a few tens of kilometres,
/// which is all segment identification needs.
class LocalProjection {
public:
    /// Projection centred on (`lon0`, `lat0`), WGS84 degrees.
    LocalProjection(double lon0, double lat0);

    /// Position in the plane of a WGS84 point.
    XY Forward(double lon, double lat) const;

    /// WGS84 point of a plane position.
    void Inverse(const XY &point, double &lon, double &lat) const;

private:
    double lon0_;  ///< centre longitude, degrees
    double lat0_;  ///< centre latitude, degrees
    double kx_;    ///< metres per degree east at the centre
    double ky_;    ///< metres per degree north
};

} // namespace core
