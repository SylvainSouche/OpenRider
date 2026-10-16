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
    for (size_t b : breaks)
        if (b > first && b <= last)
            slice.breaks.push_back(b - first);
    return slice;
}

PlaneTrack ToPlane(const RecordedTrack &track, const LocalProjection &projection, const std::vector<Rest> &rests)
{
    std::vector<bool> resting(track.points.size(), false);
    for (const Rest &rest : rests)
        for (size_t k = rest.first; k <= rest.last && k < resting.size(); ++k)
            resting[k] = true;
    bool rested = false;  // a rest since the last point kept
    const std::vector<XY> velocities = PointVelocities(track.points, projection);
    const std::vector<bool> still = NotMovingMask(track.points, MotionParams{});
    PlaneTrack plane;
    for (size_t i = 0; i < track.points.size(); ++i) {
        const or_point &point = track.points[i];
        if (resting[i]) {
            rested = true;
            continue;
        }
        if (std::isnan(point.lat) || std::isnan(point.lon) || still[i])
            continue;
        if (rested && !plane.points.empty())
            plane.breaks.push_back(plane.points.size());
        rested = false;
        const XY q = projection.Forward(point.lon, point.lat);
        plane.along.push_back(plane.points.empty() ? 0.0 : plane.along.back() + Dist(plane.points.back(), q));
        plane.points.push_back(q);
        plane.velocities.push_back(velocities[i]);
    }
    return plane;
}

} // namespace openrider::sdk
