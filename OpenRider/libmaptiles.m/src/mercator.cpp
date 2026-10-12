#include "maptiles/mercator.h"

#include <algorithm>
#include <cmath>

namespace maptiles {

namespace {
constexpr double kPi = 3.14159265358979323846;
}

double world_size(double zoom, int tile_size) { return tile_size * std::pow(2.0, zoom); }

void lonlat_to_world(double lon, double lat, double zoom, int tile_size, double &x, double &y)
{
    const double size = world_size(zoom, tile_size);
    lat = std::clamp(lat, -kMaxLatitude, kMaxLatitude);
    const double phi = lat * kPi / 180.0;
    x = (lon + 180.0) / 360.0 * size;
    y = (1.0 - std::log(std::tan(phi) + 1.0 / std::cos(phi)) / kPi) / 2.0 * size;
}

void world_to_lonlat(double x, double y, double zoom, int tile_size, double &lon, double &lat)
{
    const double size = world_size(zoom, tile_size);
    lon = x / size * 360.0 - 180.0;
    const double n = kPi * (1.0 - 2.0 * y / size);
    lat = 180.0 / kPi * std::atan(std::sinh(n));
}

int fit_zoom(double west, double south, double east, double north, double width, double height,
             int tile_size, int min_zoom, int max_zoom)
{
    for (int z = max_zoom; z > min_zoom; --z) {
        double x0, y0, x1, y1;
        lonlat_to_world(west, north, z, tile_size, x0, y0);
        lonlat_to_world(east, south, z, tile_size, x1, y1);
        if (x1 - x0 <= width && y1 - y0 <= height)
            return z;
    }
    return min_zoom;
}

} // namespace maptiles
