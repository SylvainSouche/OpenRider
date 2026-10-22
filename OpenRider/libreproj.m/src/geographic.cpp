#include "reproj/geographic.h"

#include <cmath>

namespace reproj {

bool Geographic::Forward(double lon, double lat, double &x, double &y) const
{
    x = lon;
    y = lat;
    return std::fabs(lat) <= 90.0 && std::isfinite(lon);
}

bool Geographic::Inverse(double x, double y, double &lon, double &lat) const
{
    lon = x;
    lat = y;
    return std::fabs(y) <= 90.0 && std::isfinite(x);
}

} // namespace reproj
