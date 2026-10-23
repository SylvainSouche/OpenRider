#include "reproj/transverse_mercator.h"

#include "conformal.h"

#include <cmath>

namespace reproj {

namespace {
constexpr double kDegree = 3.14159265358979323846 / 180.0;

// xi and eta of Krüger's series from the conformal sphere's xi', eta'.
void ApplySeries(const double coefficients[4], double xi_prime, double eta_prime, double sign, double &xi,
                 double &eta)
{
    xi = xi_prime;
    eta = eta_prime;
    for (int j = 1; j <= 4; ++j) {
        xi += sign * coefficients[j - 1] * std::sin(2 * j * xi_prime) * std::cosh(2 * j * eta_prime);
        eta += sign * coefficients[j - 1] * std::cos(2 * j * xi_prime) * std::sinh(2 * j * eta_prime);
    }
}
} // namespace

TransverseMercator::TransverseMercator(Ellipsoid ellipsoid, double lat0, double lon0, double k0,
                                       double false_easting, double false_northing)
    : lon0_(lon0), k0_(k0), false_easting_(false_easting), false_northing_(false_northing),
      e_(std::sqrt(ellipsoid.f * (2.0 - ellipsoid.f)))
{
    const double n = ellipsoid.f / (2.0 - ellipsoid.f);
    const double n2 = n * n, n3 = n2 * n, n4 = n3 * n;
    rectifying_ = ellipsoid.a / (1.0 + n) * (1.0 + n2 / 4.0 + n4 / 64.0);
    alpha_[0] = n / 2.0 - 2.0 * n2 / 3.0 + 5.0 * n3 / 16.0 + 41.0 * n4 / 180.0;
    alpha_[1] = 13.0 * n2 / 48.0 - 3.0 * n3 / 5.0 + 557.0 * n4 / 1440.0;
    alpha_[2] = 61.0 * n3 / 240.0 - 103.0 * n4 / 140.0;
    alpha_[3] = 49561.0 * n4 / 161280.0;
    beta_[0] = n / 2.0 - 2.0 * n2 / 3.0 + 37.0 * n3 / 96.0 - n4 / 360.0;
    beta_[1] = n2 / 48.0 + n3 / 15.0 - 437.0 * n4 / 1440.0;
    beta_[2] = 17.0 * n3 / 480.0 - 37.0 * n4 / 840.0;
    beta_[3] = 4397.0 * n4 / 161280.0;
    // On the central meridian eta' = 0 and xi' is the conformal latitude.
    const double tau_prime = ConformalTau(std::tan(lat0 * kDegree), e_);
    double eta;
    ApplySeries(alpha_, std::atan(tau_prime), 0.0, 1.0, origin_xi_, eta);
}

bool TransverseMercator::Forward(double lon, double lat, double &x, double &y) const
{
    if (!std::isfinite(lon) || !(std::fabs(lat) <= 90.0))
        return false;
    const double spread = std::remainder(lon - lon0_, 360.0);
    if (std::fabs(spread) > kMaxSpread)
        return false;
    const double lambda = spread * kDegree;
    // Conformal latitude, then the transverse Mercator of the sphere.
    const double tau = std::tan(std::fabs(lat) >= 90.0 ? std::copysign(89.9999999, lat) * kDegree : lat * kDegree);
    const double tau_prime = ConformalTau(tau, e_);
    const double xi_prime = std::atan2(tau_prime, std::cos(lambda));
    const double eta_prime = std::asinh(std::sin(lambda) / std::hypot(tau_prime, std::cos(lambda)));
    double xi, eta;
    ApplySeries(alpha_, xi_prime, eta_prime, 1.0, xi, eta);
    x = false_easting_ + k0_ * rectifying_ * eta;
    y = false_northing_ + k0_ * rectifying_ * (xi - origin_xi_);
    return true;
}

bool TransverseMercator::Inverse(double x, double y, double &lon, double &lat) const
{
    const double xi = ((y - false_northing_) / k0_ / rectifying_) + origin_xi_;
    const double eta = (x - false_easting_) / k0_ / rectifying_;
    if (!std::isfinite(xi) || !std::isfinite(eta) || std::fabs(eta) > 3.0)
        return false;
    double xi_prime, eta_prime;
    ApplySeries(beta_, xi, eta, -1.0, xi_prime, eta_prime);
    const double tau_prime = std::sin(xi_prime) / std::hypot(std::sinh(eta_prime), std::cos(xi_prime));
    const double lambda = std::atan2(std::sinh(eta_prime), std::cos(xi_prime));
    if (std::fabs(lambda) > kMaxSpread * kDegree)
        return false;
    lat = std::atan(GeographicTau(tau_prime, e_)) / kDegree;
    lon = lon0_ + lambda / kDegree;
    return true;
}

} // namespace reproj
