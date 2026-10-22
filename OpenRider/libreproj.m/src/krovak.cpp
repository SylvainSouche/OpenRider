#include "reproj/krovak.h"

#include <cmath>

namespace reproj {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kDegree = kPi / 180.0;
} // namespace

Krovak::Krovak(Ellipsoid ellipsoid, double lat0, double lon0, double alpha, double lat_ts, double k0,
               double false_easting, double false_northing)
    : e_(std::sqrt(ellipsoid.f * (2.0 - ellipsoid.f))), lon0_(lon0), false_easting_(false_easting),
      false_northing_(false_northing), alpha_c_(alpha * kDegree), lat_p_(lat_ts * kDegree)
{
    const double e2 = e_ * e_, phi_c = lat0 * kDegree, sin_c = std::sin(phi_c);
    const double a_bar = ellipsoid.a * std::sqrt(1.0 - e2) / (1.0 - e2 * sin_c * sin_c);
    b_ = std::sqrt(1.0 + e2 * std::pow(std::cos(phi_c), 4) / (1.0 - e2));
    const double gamma0 = std::asin(sin_c / b_);
    t0_ = std::tan(kPi / 4.0 + gamma0 / 2.0) * std::pow((1.0 + e_ * sin_c) / (1.0 - e_ * sin_c), e_ * b_ / 2.0) /
          std::pow(std::tan(kPi / 4.0 + phi_c / 2.0), b_);
    n_ = std::sin(lat_p_);
    r0_ = k0 * a_bar / std::tan(lat_p_);
}

bool Krovak::Forward(double lon, double lat, double &x, double &y) const
{
    if (!std::isfinite(lon) || !(std::fabs(lat) < 90.0))
        return false;
    const double phi = lat * kDegree, sin_phi = std::sin(phi);
    const double u = 2.0 * (std::atan(t0_ * std::pow(std::tan(phi / 2.0 + kPi / 4.0), b_) /
                                      std::pow((1.0 + e_ * sin_phi) / (1.0 - e_ * sin_phi), e_ * b_ / 2.0)) -
                            kPi / 4.0);
    const double v = b_ * std::remainder(lon0_ - lon, 360.0) * kDegree;
    const double t = std::asin(std::cos(alpha_c_) * std::sin(u) + std::sin(alpha_c_) * std::cos(u) * std::cos(v));
    const double d = std::asin(std::cos(u) * std::sin(v) / std::cos(t));
    const double theta = n_ * d;
    const double r = r0_ * std::pow(std::tan(kPi / 4.0 + lat_p_ / 2.0), n_) / std::pow(std::tan(t / 2.0 + kPi / 4.0), n_);
    if (!std::isfinite(r))
        return false;
    // Southing r cos(theta), westing r sin(theta); East North negates both.
    x = false_easting_ - r * std::sin(theta);
    y = false_northing_ - r * std::cos(theta);
    return true;
}

bool Krovak::Inverse(double x, double y, double &lon, double &lat) const
{
    const double southing = false_northing_ - y, westing = false_easting_ - x;
    const double r = std::hypot(southing, westing);
    if (r == 0.0)
        return false;
    const double theta = std::atan2(westing, southing);
    const double d = theta / n_;
    const double t = 2.0 * (std::atan(std::pow(r0_ / r, 1.0 / n_) * std::tan(kPi / 4.0 + lat_p_ / 2.0)) - kPi / 4.0);
    const double u = std::asin(std::cos(alpha_c_) * std::sin(t) - std::sin(alpha_c_) * std::cos(t) * std::cos(d));
    const double v = std::asin(std::cos(t) * std::sin(d) / std::cos(u));
    double phi = u;
    for (int i = 0; i < 20; ++i) {
        const double sin_phi = std::sin(phi);
        const double next = 2.0 * (std::atan(std::pow(t0_, -1.0 / b_) * std::pow(std::tan(u / 2.0 + kPi / 4.0), 1.0 / b_) *
                                             std::pow((1.0 + e_ * sin_phi) / (1.0 - e_ * sin_phi), e_ / 2.0)) -
                                   kPi / 4.0);
        const bool done = std::fabs(next - phi) < 1e-14;
        phi = next;
        if (done)
            break;
    }
    lat = phi / kDegree;
    lon = lon0_ - v / b_ / kDegree;
    return std::isfinite(lon) && std::isfinite(lat);
}

} // namespace reproj
