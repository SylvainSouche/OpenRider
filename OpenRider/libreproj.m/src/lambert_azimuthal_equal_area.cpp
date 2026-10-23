#include "reproj/lambert_azimuthal_equal_area.h"

#include <algorithm>
#include <cmath>

namespace reproj {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kDegree = kPi / 180.0;
} // namespace

double LambertAzimuthalEqualArea::Q(double phi) const
{
    const double sin_phi = std::sin(phi), e2 = e_ * e_;
    if (e_ == 0.0)
        return 2.0 * sin_phi;
    return (1.0 - e2) * (sin_phi / (1.0 - e2 * sin_phi * sin_phi) - std::log((1.0 - e_ * sin_phi) / (1.0 + e_ * sin_phi)) / (2.0 * e_));
}

LambertAzimuthalEqualArea::LambertAzimuthalEqualArea(Ellipsoid ellipsoid, double lat0, double lon0,
                                                     double false_easting, double false_northing)
    : e_(std::sqrt(ellipsoid.f * (2.0 - ellipsoid.f))), lon0_(lon0), false_easting_(false_easting),
      false_northing_(false_northing)
{
    const double phi0 = lat0 * kDegree;
    q_pole_ = Q(kPi / 2.0);
    rq_ = ellipsoid.a * std::sqrt(q_pole_ / 2.0);
    beta0_ = std::asin(std::clamp(Q(phi0) / q_pole_, -1.0, 1.0));
    const double cos_beta0 = std::cos(beta0_);
    d_ = std::fabs(cos_beta0) < 1e-12
             ? 1.0
             : ellipsoid.a * std::cos(phi0) / std::sqrt(1.0 - e_ * e_ * std::sin(phi0) * std::sin(phi0)) / (rq_ * cos_beta0);
}

bool LambertAzimuthalEqualArea::Forward(double lon, double lat, double &x, double &y) const
{
    if (!std::isfinite(lon) || !(std::fabs(lat) <= 90.0))
        return false;
    const double beta = std::asin(std::clamp(Q(lat * kDegree) / q_pole_, -1.0, 1.0));
    const double lambda = std::remainder(lon - lon0_, 360.0) * kDegree;
    const double denominator = 1.0 + std::sin(beta0_) * std::sin(beta) + std::cos(beta0_) * std::cos(beta) * std::cos(lambda);
    if (denominator < 1e-12)
        return false;
    const double b = rq_ * std::sqrt(2.0 / denominator);
    x = false_easting_ + b * d_ * std::cos(beta) * std::sin(lambda);
    y = false_northing_ + b / d_ * (std::cos(beta0_) * std::sin(beta) - std::sin(beta0_) * std::cos(beta) * std::cos(lambda));
    return true;
}

bool LambertAzimuthalEqualArea::Inverse(double x, double y, double &lon, double &lat) const
{
    const double east = x - false_easting_, north = y - false_northing_;
    const double rho = std::hypot(east / d_, d_ * north);
    if (rho == 0.0) {
        lon = lon0_;
        lat = beta0_ / kDegree;  // the centre (beta0 = phi0 at the poles and the equator)
        return true;
    }
    if (rho > 2.0 * rq_)
        return false;
    const double c = 2.0 * std::asin(rho / (2.0 * rq_));
    const double beta = std::asin(std::cos(c) * std::sin(beta0_) + d_ * north * std::sin(c) * std::cos(beta0_) / rho);
    const double lambda = std::atan2(east * std::sin(c),
                                     d_ * rho * std::cos(beta0_) * std::cos(c) - d_ * d_ * north * std::sin(beta0_) * std::sin(c));
    // Latitude from q (Snyder, eq. 3-16), iterated.
    const double q = q_pole_ * std::sin(beta), e2 = e_ * e_;
    double phi = std::asin(q / 2.0);
    for (int i = 0; i < 25 && e_ > 0.0; ++i) {
        const double sin_phi = std::sin(phi), one = 1.0 - e2 * sin_phi * sin_phi;
        const double next = phi + one * one / (2.0 * std::cos(phi)) *
                                      (q / (1.0 - e2) - sin_phi / one + std::log((1.0 - e_ * sin_phi) / (1.0 + e_ * sin_phi)) / (2.0 * e_));
        const bool done = std::fabs(next - phi) < 1e-14;
        phi = next;
        if (done)
            break;
    }
    lat = phi / kDegree;
    lon = lon0_ + lambda / kDegree;
    return std::isfinite(lon) && std::isfinite(lat);
}

} // namespace reproj
