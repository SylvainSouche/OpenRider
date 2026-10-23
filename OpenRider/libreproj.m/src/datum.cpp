#include "reproj/datum.h"

#include <cmath>

namespace reproj {

namespace {

constexpr double kDegree = 3.14159265358979323846 / 180.0;
constexpr double kArcSecond = kDegree / 3600.0;

// Geodetic (height 0) to geocentric coordinates.
void ToGeocentric(const Ellipsoid &ellipsoid, double lon, double lat, double xyz[3])
{
    const double e2 = ellipsoid.f * (2.0 - ellipsoid.f);
    const double phi = lat * kDegree, lambda = lon * kDegree;
    const double nu = ellipsoid.a / std::sqrt(1.0 - e2 * std::sin(phi) * std::sin(phi));
    xyz[0] = nu * std::cos(phi) * std::cos(lambda);
    xyz[1] = nu * std::cos(phi) * std::sin(lambda);
    xyz[2] = nu * (1.0 - e2) * std::sin(phi);
}

// Geocentric to geodetic coordinates (the height is dropped); a few
// iterations reach a tenth of a millimetre.
void FromGeocentric(const Ellipsoid &ellipsoid, const double xyz[3], double &lon, double &lat)
{
    const double e2 = ellipsoid.f * (2.0 - ellipsoid.f);
    const double p = std::hypot(xyz[0], xyz[1]);
    double phi = std::atan2(xyz[2], p * (1.0 - e2));
    for (int i = 0; i < 6; ++i) {
        const double nu = ellipsoid.a / std::sqrt(1.0 - e2 * std::sin(phi) * std::sin(phi));
        const double height = p / std::cos(phi) - nu;
        phi = std::atan2(xyz[2], p * (1.0 - e2 * nu / (nu + height)));
    }
    lon = std::atan2(xyz[1], xyz[0]) / kDegree;
    lat = phi / kDegree;
}

} // namespace

void ToWgs84(const Datum &datum, double &lon, double &lat)
{
    if (!datum.shifted)
        return;
    const double *p = datum.towgs84;
    const double rx = p[3] * kArcSecond, ry = p[4] * kArcSecond, rz = p[5] * kArcSecond, scale = 1.0 + p[6] * 1e-6;
    double in[3], out[3];
    ToGeocentric(datum.ellipsoid, lon, lat, in);
    out[0] = p[0] + scale * (in[0] - rz * in[1] + ry * in[2]);
    out[1] = p[1] + scale * (rz * in[0] + in[1] - rx * in[2]);
    out[2] = p[2] + scale * (-ry * in[0] + rx * in[1] + in[2]);
    FromGeocentric(kWgs84, out, lon, lat);
}

void FromWgs84(const Datum &datum, double &lon, double &lat)
{
    if (!datum.shifted)
        return;
    const double *p = datum.towgs84;
    const double rx = p[3] * kArcSecond, ry = p[4] * kArcSecond, rz = p[5] * kArcSecond, scale = 1.0 + p[6] * 1e-6;
    double in[3], out[3];
    ToGeocentric(kWgs84, lon, lat, in);
    // The inverse: translation removed, scale divided, rotation transposed.
    const double x = (in[0] - p[0]) / scale, y = (in[1] - p[1]) / scale, z = (in[2] - p[2]) / scale;
    out[0] = x + rz * y - ry * z;
    out[1] = -rz * x + y + rx * z;
    out[2] = ry * x - rx * y + z;
    FromGeocentric(datum.ellipsoid, out, lon, lat);
}

} // namespace reproj
