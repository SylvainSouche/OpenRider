#include <openrider/sdk/velocity.h>

#include <cmath>
#include <limits>

namespace openrider::sdk {

std::vector<XY> PointVelocities(const std::vector<or_point> &points, const LocalProjection &projection, double reach_m)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    std::vector<XY> velocities(points.size(), XY{nan, nan});
    // The points with a position, in the plane, and the path along them.
    std::vector<size_t> located;
    std::vector<XY> plane;
    std::vector<double> along;
    for (size_t i = 0; i < points.size(); ++i) {
        if (std::isnan(points[i].lat) || std::isnan(points[i].lon))
            continue;
        const XY q = projection.Forward(points[i].lon, points[i].lat);
        along.push_back(plane.empty() ? 0.0 : along.back() + Dist(plane.back(), q));
        located.push_back(i);
        plane.push_back(q);
    }
    size_t before = 0, after = 0;
    for (size_t k = 0; k < plane.size(); ++k) {
        while (before + 1 < k && along[k] - along[before + 1] >= reach_m)
            ++before;
        if (after < k)
            after = k;
        while (after + 1 < plane.size() && along[after] - along[k] < reach_m)
            ++after;
        const XY &a = plane[before], &b = plane[after];
        const double dx = b.x - a.x, dy = b.y - a.y, length = std::hypot(dx, dy);
        const or_point &point = points[located[k]];
        const double dt = points[located[after]].t_s - points[located[before]].t_s;
        const double speed = !std::isnan(point.speed_ms) ? point.speed_ms : dt > 0.0 ? (along[after] - along[before]) / dt : 0.0;
        velocities[located[k]] = length > 0.0 ? XY{speed * dx / length, speed * dy / length} : XY{0.0, 0.0};
    }
    return velocities;
}

} // namespace openrider::sdk
