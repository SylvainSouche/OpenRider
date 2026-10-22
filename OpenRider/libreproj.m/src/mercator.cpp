#include "reproj/mercator.h"

#include "conformal.h"

#include <cmath>

namespace reproj {

namespace {
constexpr double kDegree = 3.14159265358979323846 / 180.0;
} // namespace

Mercator::Mercator(Ellipsoid ellipsoid, double lon0, double k0, double false_easting, double false_northing)
    : a_(ellipsoid.a), e_(std::sqrt(ellipsoid.f * (2.0 - ellipsoid.f))), lon0_(lon0), k0_(k0),
      false_easting_(false_easting), false_northing_(false_northing)
{
}

bool Mercator::Forward(double lon, double lat, double &x, double &y) const
{
    if (!std::isfinite(lon) || !(std::fabs(lat) < kMaxLatitude))
        return false;
    // y is the isometric latitude: asinh of the conformal latitude's tangent.
    x = false_easting_ + a_ * k0_ * (lon - lon0_) * kDegree;
    y = false_northing_ + a_ * k0_ * std::asinh(ConformalTau(std::tan(lat * kDegree), e_));
    return true;
}

bool Mercator::Inverse(double x, double y, double &lon, double &lat) const
{
    lon = lon0_ + (x - false_easting_) / (a_ * k0_) / kDegree;
    lat = std::atan(GeographicTau(std::sinh((y - false_northing_) / (a_ * k0_)), e_)) / kDegree;
    return std::isfinite(lon) && std::isfinite(lat);
}

} // namespace reproj
