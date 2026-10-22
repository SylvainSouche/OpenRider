#include "reproj/oblique_stereographic.h"

#include <cmath>

namespace reproj {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kDegree = kPi / 180.0;
} // namespace

ObliqueStereographic::ObliqueStereographic(Ellipsoid ellipsoid, double lat0, double lon0, double k0,
                                           double false_easting, double false_northing)
    : e_(std::sqrt(ellipsoid.f * (2.0 - ellipsoid.f))), lon0_(lon0), false_easting_(false_easting),
      false_northing_(false_northing)
{
    const double e2 = e_ * e_, phi0 = lat0 * kDegree, sin0 = std::sin(phi0);
    const double rho0 = ellipsoid.a * (1.0 - e2) / std::pow(1.0 - e2 * sin0 * sin0, 1.5);
    const double nu0 = ellipsoid.a / std::sqrt(1.0 - e2 * sin0 * sin0);
    two_r_k0_ = 2.0 * std::sqrt(rho0 * nu0) * k0;
    n_ = std::sqrt(1.0 + e2 * std::pow(std::cos(phi0), 4) / (1.0 - e2));
    const double s1 = (1.0 + sin0) / (1.0 - sin0), s2 = (1.0 - e_ * sin0) / (1.0 + e_ * sin0);
    const double w1 = std::pow(s1 * std::pow(s2, e_), n_);
    const double sin_chi = (w1 - 1.0) / (w1 + 1.0);
    c_ = (n_ + sin0) * (1.0 - sin_chi) / ((n_ - sin0) * (1.0 + sin_chi));
    const double w2 = c_ * w1;
    chi0_ = std::asin((w2 - 1.0) / (w2 + 1.0));
}

bool ObliqueStereographic::Forward(double lon, double lat, double &x, double &y) const
{
    if (!std::isfinite(lon) || !(std::fabs(lat) < 90.0))
        return false;
    const double phi = lat * kDegree, sin_phi = std::sin(phi);
    const double lambda = n_ * std::remainder(lon - lon0_, 360.0) * kDegree;
    const double sa = (1.0 + sin_phi) / (1.0 - sin_phi), sb = (1.0 - e_ * sin_phi) / (1.0 + e_ * sin_phi);
    const double w = c_ * std::pow(sa * std::pow(sb, e_), n_);
    const double chi = std::asin((w - 1.0) / (w + 1.0));
    const double b = 1.0 + std::sin(chi) * std::sin(chi0_) + std::cos(chi) * std::cos(chi0_) * std::cos(lambda);
    if (b < 1e-12)
        return false;
    x = false_easting_ + two_r_k0_ * std::cos(chi) * std::sin(lambda) / b;
    y = false_northing_ +
        two_r_k0_ * (std::sin(chi) * std::cos(chi0_) - std::cos(chi) * std::sin(chi0_) * std::cos(lambda)) / b;
    return true;
}

bool ObliqueStereographic::Inverse(double x, double y, double &lon, double &lat) const
{
    const double east = x - false_easting_, north = y - false_northing_;
    const double g = two_r_k0_ * std::tan(kPi / 4.0 - chi0_ / 2.0);
    const double h = 2.0 * two_r_k0_ * std::tan(chi0_) + g;
    const double i = std::atan2(east, h + north);
    const double j = std::atan2(east, g - north) - i;
    const double chi = chi0_ + 2.0 * std::atan((north - east * std::tan(j / 2.0)) / two_r_k0_);
    const double lambda = j + 2.0 * i;
    // Back from the conformal sphere: isometric latitude, then iterations.
    const double psi = 0.5 * std::log((1.0 + std::sin(chi)) / (c_ * (1.0 - std::sin(chi)))) / n_;
    double phi = 2.0 * std::atan(std::exp(psi)) - kPi / 2.0;
    for (int k = 0; k < 15; ++k) {
        const double sin_phi = std::sin(phi);
        const double psi_k = std::log(std::tan(phi / 2.0 + kPi / 4.0) *
                                      std::pow((1.0 - e_ * sin_phi) / (1.0 + e_ * sin_phi), e_ / 2.0));
        const double next = phi - (psi_k - psi) * std::cos(phi) * (1.0 - e_ * e_ * sin_phi * sin_phi) / (1.0 - e_ * e_);
        const bool done = std::fabs(next - phi) < 1e-14;
        phi = next;
        if (done)
            break;
    }
    lat = phi / kDegree;
    lon = lon0_ + lambda / n_ / kDegree;
    return std::isfinite(lon) && std::isfinite(lat);
}

} // namespace reproj
