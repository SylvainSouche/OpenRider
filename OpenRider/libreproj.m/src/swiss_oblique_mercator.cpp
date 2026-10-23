#include "reproj/swiss_oblique_mercator.h"

#include <cmath>

namespace reproj {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kDegree = kPi / 180.0;
} // namespace

SwissObliqueMercator::SwissObliqueMercator(Ellipsoid ellipsoid, double lat0, double lon0, double k0,
                                           double false_easting, double false_northing)
    : e_(std::sqrt(ellipsoid.f * (2.0 - ellipsoid.f))), lon0_(lon0), false_easting_(false_easting),
      false_northing_(false_northing)
{
    const double e2 = e_ * e_, phi0 = lat0 * kDegree, sin0 = std::sin(phi0);
    alpha_ = std::sqrt(1.0 + e2 / (1.0 - e2) * std::pow(std::cos(phi0), 4));
    b0_ = std::asin(sin0 / alpha_);
    k_ = std::log(std::tan(kPi / 4.0 + b0_ / 2.0)) - alpha_ * std::log(std::tan(kPi / 4.0 + phi0 / 2.0)) +
         alpha_ * e_ * std::atanh(e_ * sin0);
    r_k0_ = ellipsoid.a * std::sqrt(1.0 - e2) / (1.0 - e2 * sin0 * sin0) * k0;
}

bool SwissObliqueMercator::Forward(double lon, double lat, double &x, double &y) const
{
    if (!std::isfinite(lon) || !(std::fabs(lat) < 90.0))
        return false;
    const double phi = lat * kDegree;
    // Onto the sphere...
    const double s = alpha_ * std::log(std::tan(kPi / 4.0 + phi / 2.0)) - alpha_ * e_ * std::atanh(e_ * std::sin(phi)) + k_;
    const double b = 2.0 * (std::atan(std::exp(s)) - kPi / 4.0);
    const double l = alpha_ * std::remainder(lon - lon0_, 360.0) * kDegree;
    // ...rotated so that the origin lies on the equator...
    const double l_bar = std::atan2(std::sin(l), std::sin(b0_) * std::tan(b) + std::cos(b0_) * std::cos(l));
    const double sin_b_bar = std::cos(b0_) * std::sin(b) - std::sin(b0_) * std::cos(b) * std::cos(l);
    if (std::fabs(sin_b_bar) >= 1.0 - 1e-15)
        return false;
    // ...and Mercator of the rotated sphere.
    x = false_easting_ + r_k0_ * l_bar;
    y = false_northing_ + r_k0_ * std::atanh(sin_b_bar);
    return true;
}

bool SwissObliqueMercator::Inverse(double x, double y, double &lon, double &lat) const
{
    const double l_bar = (x - false_easting_) / r_k0_;
    const double b_bar = 2.0 * (std::atan(std::exp((y - false_northing_) / r_k0_)) - kPi / 4.0);
    const double b = std::asin(std::cos(b0_) * std::sin(b_bar) + std::sin(b0_) * std::cos(b_bar) * std::cos(l_bar));
    const double l = std::atan2(std::sin(l_bar), std::cos(b0_) * std::cos(l_bar) - std::sin(b0_) * std::tan(b_bar));
    const double isometric = (std::log(std::tan(kPi / 4.0 + b / 2.0)) - k_) / alpha_;
    double phi = b;
    for (int i = 0; i < 20; ++i) {
        const double next = 2.0 * std::atan(std::exp(isometric + e_ * std::atanh(e_ * std::sin(phi)))) - kPi / 2.0;
        const bool done = std::fabs(next - phi) < 1e-14;
        phi = next;
        if (done)
            break;
    }
    lat = phi / kDegree;
    lon = lon0_ + l / alpha_ / kDegree;
    return std::isfinite(lon) && std::isfinite(lat);
}

} // namespace reproj
