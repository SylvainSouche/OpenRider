#include <openrider/sdk/plane_track.h>

#include <openrider/sdk/motion.h>
#include <openrider/sdk/velocity.h>

#include <cmath>

namespace openrider::sdk {

PlaneTrack PlaneTrack::Slice(size_t first, size_t last) const
{
    PlaneTrack slice;
    slice.points.assign(points.begin() + static_cast<long>(first), points.begin() + static_cast<long>(last) + 1);
    slice.velocities.assign(velocities.begin() + static_cast<long>(first), velocities.begin() + static_cast<long>(last) + 1);
    slice.along.assign(along.begin() + static_cast<long>(first), along.begin() + static_cast<long>(last) + 1);
    return slice;
}

PlaneTrack ToPlane(const RecordedTrack &track, const LocalProjection &projection)
{
    const std::vector<XY> velocities = PointVelocities(track.points, projection);
    const std::vector<bool> still = NotMovingMask(track.points, MotionParams{});
    PlaneTrack plane;
    for (size_t i = 0; i < track.points.size(); ++i) {
        const or_point &point = track.points[i];
        if (std::isnan(point.lat) || std::isnan(point.lon) || still[i])
            continue;
        const XY q = projection.Forward(point.lon, point.lat);
        plane.along.push_back(plane.points.empty() ? 0.0 : plane.along.back() + Dist(plane.points.back(), q));
        plane.points.push_back(q);
        plane.velocities.push_back(velocities[i]);
    }
    return plane;
}

} // namespace openrider::sdk
