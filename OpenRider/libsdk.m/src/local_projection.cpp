#include <openrider/sdk/local_projection.h>

#include <cmath>
#include <numbers>

namespace openrider::sdk {

LocalProjection::LocalProjection(double lon0, double lat0)
    : lon0_(lon0), lat0_(lat0), kx_(111320.0 * std::cos(lat0 * std::numbers::pi / 180.0)), ky_(110540.0)
{
}

XY LocalProjection::Forward(double lon, double lat) const { return {(lon - lon0_) * kx_, (lat - lat0_) * ky_}; }

void LocalProjection::Inverse(const XY &point, double &lon, double &lat) const
{
    lon = lon0_ + point.x / kx_;
    lat = lat0_ + point.y / ky_;
}

} // namespace openrider::sdk
