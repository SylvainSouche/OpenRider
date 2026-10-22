#include "reproj/lambert_conformal_conic.h"

#include <cmath>

namespace reproj {

namespace {
constexpr double kDegree = 3.14159265358979323846 / 180.0;
constexpr double kHalfPi = 3.14159265358979323846 / 2.0;
} // namespace

double LambertConformalConic::T(double phi) const
{
    const double e_sin = e_ * std::sin(phi);
    return std::tan(kHalfPi / 2.0 - phi / 2.0) / std::pow((1.0 - e_sin) / (1.0 + e_sin), e_ / 2.0);
}

LambertConformalConic::LambertConformalConic(Ellipsoid ellipsoid, double lat0, double lon0, double lat1,
                                             double lat2, double k0, double false_easting, double false_northing)
    : a_(ellipsoid.a), e_(std::sqrt(ellipsoid.f * (2.0 - ellipsoid.f))), lon0_(lon0),
      false_easting_(false_easting), false_northing_(false_northing)
{
    auto m = [this](double phi) { return std::cos(phi) / std::sqrt(1.0 - e_ * e_ * std::sin(phi) * std::sin(phi)); };
    const double phi1 = lat1 * kDegree, phi2 = lat2 * kDegree;
    const double m1 = m(phi1), m2 = m(phi2), t1 = T(phi1), t2 = T(phi2);
    n_ = phi1 == phi2 ? std::sin(phi1) : (std::log(m1) - std::log(m2)) / (std::log(t1) - std::log(t2));
    f_ = a_ * k0 * m1 / (n_ * std::pow(t1, n_));
    r0_ = f_ * std::pow(T(lat0 * kDegree), n_);
}

bool LambertConformalConic::Forward(double lon, double lat, double &x, double &y) const
{
    // The pole opposite the cone's apex lies at infinity.
    if (!std::isfinite(lon) || !(std::fabs(lat) <= 90.0) || (n_ > 0.0 ? lat <= -90.0 + 1e-9 : lat >= 90.0 - 1e-9))
        return false;
    const double phi = lat * kDegree;
    const double r = std::fabs(lat) >= 90.0 ? 0.0 : f_ * std::pow(T(phi), n_);
    const double theta = n_ * std::remainder(lon - lon0_, 360.0) * kDegree;
    x = false_easting_ + r * std::sin(theta);
    y = false_northing_ + r0_ - r * std::cos(theta);
    return std::isfinite(x) && std::isfinite(y);
}

bool LambertConformalConic::Inverse(double x, double y, double &lon, double &lat) const
{
    const double dx = x - false_easting_, dy = r0_ - (y - false_northing_);
    const double sign = n_ < 0.0 ? -1.0 : 1.0;
    const double r = sign * std::hypot(dx, dy);
    if (r == 0.0) {
        lon = lon0_;
        lat = sign * 90.0;
        return true;
    }
    const double t = std::pow(r / f_, 1.0 / n_);
    const double theta = std::atan2(sign * dx, sign * dy);
    double phi = kHalfPi - 2.0 * std::atan(t);
    for (int i = 0; i < 15; ++i) {
        const double e_sin = e_ * std::sin(phi);
        const double next = kHalfPi - 2.0 * std::atan(t * std::pow((1.0 - e_sin) / (1.0 + e_sin), e_ / 2.0));
        const bool done = std::fabs(next - phi) < 1e-15;
        phi = next;
        if (done)
            break;
    }
    lat = phi / kDegree;
    lon = lon0_ + theta / n_ / kDegree;
    return std::isfinite(lon) && std::isfinite(lat);
}

} // namespace reproj
