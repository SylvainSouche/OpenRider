/// @file
/// Velocity of recorded points (module-level utilities).
// @impl 0f87-6acf-6ab9-9a82
#pragma once

#include <openrider/input.h>
#include <openrider/sdk/local_projection.h>
#include <openrider/sdk/plane_geometry.h>

#include <vector>

namespace openrider::sdk {

/// Velocity (m/s, east and north in `projection`'s plane) of each recorded
/// point: its recorded speed (from its positions where none is recorded),
/// along its direction of travel -- from the position `reach_m` of path
/// before it to the one `reach_m` after it, so that a few metres of GPS
/// wander do not turn it. NaN for a point without position; (0, 0) for a
/// point recorded at speed 0, or with no movement around it.
std::vector<XY> PointVelocities(const std::vector<or_point> &points, const LocalProjection &projection,
                                double reach_m = 5.0);

} // namespace openrider::sdk
