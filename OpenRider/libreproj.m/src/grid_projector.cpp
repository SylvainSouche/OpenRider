#include "reproj/grid_projector.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace reproj {

GridProjector::GridProjector(Transformer transformer, const Box &box, double tolerance, int max_cells)
    : transformer_(std::move(transformer)), box_(box), mesh_(BuildWarpMesh(transformer_, box, tolerance, max_cells))
{
    usable_ = mesh_.cells > 0 && mesh_.max_error <= tolerance && box.x1 != box.x0 && box.y1 != box.y0;
}

bool GridProjector::Project(double x, double y, double &out_x, double &out_y) const
{
    if (usable_) {
        const int n = mesh_.cells;
        const double u = (x - box_.x0) / (box_.x1 - box_.x0) * n, v = (y - box_.y0) / (box_.y1 - box_.y0) * n;
        if (u >= 0.0 && v >= 0.0 && u <= n && v <= n) {
            const int column = std::min(n - 1, static_cast<int>(u)), row = std::min(n - 1, static_cast<int>(v));
            const double fx = u - column, fy = v - row;
            const MeshVertex *top = &mesh_.vertices[static_cast<size_t>(row * (n + 1) + column)];
            const MeshVertex *bottom = top + (n + 1);
            const double px = (top[0].x * (1 - fx) + top[1].x * fx) * (1 - fy) + (bottom[0].x * (1 - fx) + bottom[1].x * fx) * fy;
            const double py = (top[0].y * (1 - fx) + top[1].y * fx) * (1 - fy) + (bottom[0].y * (1 - fx) + bottom[1].y * fx) * fy;
            if (std::isfinite(px) && std::isfinite(py)) {
                out_x = px;
                out_y = py;
                return true;
            }
        }
    }
    return transformer_.Transform(x, y, out_x, out_y);
}

double ScaleFactor(const Crs &crs, double lon, double lat)
{
    // Two points 0.001 degree apart along the meridian, projected; their
    // distance on the ellipsoid is the meridian radius times the angle.
    constexpr double kStep = 0.001, kDegree = 3.14159265358979323846 / 180.0;
    const double e2 = crs.datum.ellipsoid.f * (2.0 - crs.datum.ellipsoid.f);
    const double sin_phi = std::sin(lat * kDegree);
    const double meridian_radius = crs.datum.ellipsoid.a * (1.0 - e2) / std::pow(1.0 - e2 * sin_phi * sin_phi, 1.5);
    double x0, y0, x1, y1;
    if (!crs.projection->Forward(lon, lat - kStep / 2, x0, y0) || !crs.projection->Forward(lon, lat + kStep / 2, x1, y1))
        return 1.0;
    return std::hypot(x1 - x0, y1 - y0) / (meridian_radius * kStep * kDegree);
}

} // namespace reproj
