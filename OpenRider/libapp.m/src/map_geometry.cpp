#include "app/map_geometry.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace app {

namespace {

// Width of the world at zoom 0 over 256 pixels: the equator, in metres or
// degrees.
double WorldWidth(const reproj::Crs &display)
{
    return display.geographic ? 360.0 : 2.0 * 3.14159265358979323846 * 6378137.0;
}

// The WGS84 coordinate system, built in to every registry.
const reproj::Crs &Wgs84()
{
    static const reproj::Crs wgs84 = reproj::CrsRegistry().Find(4326);
    return wgs84;
}

} // namespace

double ResolutionAtZoom(const reproj::Crs &display, double zoom)
{
    return WorldWidth(display) / (256.0 * std::pow(2.0, zoom));
}

double ZoomAtResolution(const reproj::Crs &display, double resolution)
{
    return std::log2(WorldWidth(display) / (256.0 * resolution));
}

MapGeometry::MapGeometry(const MapViewModel &view_model)
    : MapGeometry(view_model.display, view_model.Camera(), view_model.width, view_model.height)
{
}

MapGeometry::MapGeometry(const reproj::Crs &display, const MapCamera &camera, double width, double height)
    : display_(display), to_wgs84_(display, Wgs84()), resolution_(ResolutionAtZoom(display, camera.zoom)),
      width_(width), height_(height), from_wgs84_(Wgs84(), display)
{
    valid_ = display_.Valid() && from_wgs84_.Transform(camera.lon, camera.lat, centre_x_, centre_y_);
    if (!valid_)
        return;
    // The view's WGS84 box: its border and centre, unprojected.
    double west = 1e9, east = -1e9, south = 1e9, north = -1e9;
    bool found = false;
    constexpr int kSteps = 8;
    for (int i = 0; i <= kSteps; ++i)
        for (int j = 0; j <= kSteps; ++j) {
            if (i != 0 && i != kSteps && j != 0 && j != kSteps && !(i == kSteps / 2 && j == kSteps / 2))
                continue;
            double lon, lat;
            if (!ToLonlat(width_ * i / kSteps, height_ * j / kSteps, lon, lat))
                continue;
            west = std::min(west, lon);
            east = std::max(east, lon);
            south = std::min(south, lat);
            north = std::max(north, lat);
            found = true;
        }
    if (found)
        view_box_ = {west, std::max(-90.0, south), east, std::min(90.0, north)};
}

void MapGeometry::DisplayToScreen(double x, double y, double &screen_x, double &screen_y) const
{
    screen_x = (x - centre_x_) / resolution_ + width_ / 2.0;
    screen_y = (centre_y_ - y) / resolution_ + height_ / 2.0;
}

void MapGeometry::ScreenToDisplay(double screen_x, double screen_y, double &x, double &y) const
{
    x = centre_x_ + (screen_x - width_ / 2.0) * resolution_;
    y = centre_y_ - (screen_y - height_ / 2.0) * resolution_;
}

bool MapGeometry::ToScreen(double lon, double lat, double &screen_x, double &screen_y) const
{
    if (!valid_)
        return false;
    if (!to_display_) {
        // Points are drawn through an interpolation grid over the view and
        // a margin around it, to a quarter of a pixel.
        const double margin_x = (view_box_.east - view_box_.west) * 0.25;
        const double margin_y = (view_box_.north - view_box_.south) * 0.25;
        const reproj::Box grid_box{view_box_.west - margin_x, std::min(90.0, view_box_.north + margin_y),
                                   view_box_.east + margin_x, std::max(-90.0, view_box_.south - margin_y)};
        to_display_ = std::make_shared<reproj::GridProjector>(from_wgs84_, grid_box, 0.25 * resolution_, 32);
    }
    double x, y;
    if (!to_display_->Project(lon, lat, x, y))
        return false;
    DisplayToScreen(x, y, screen_x, screen_y);
    return true;
}

bool MapGeometry::ToLonlat(double screen_x, double screen_y, double &lon, double &lat) const
{
    double x, y;
    ScreenToDisplay(screen_x, screen_y, x, y);
    return to_wgs84_.Transform(x, y, lon, lat);
}

MapCamera MapGeometry::ZoomedAt(const MapCamera &camera, double zoom, double screen_x, double screen_y) const
{
    // The point under the mouse keeps its display coordinates; the centre
    // moves so that it stays at the same screen offset at the new scale.
    MapCamera zoomed = camera;
    zoomed.zoom = zoom;
    double x, y;
    ScreenToDisplay(screen_x, screen_y, x, y);
    const double resolution = ResolutionAtZoom(display_, zoom);
    const double centre_x = x - (screen_x - width_ / 2.0) * resolution;
    const double centre_y = y + (screen_y - height_ / 2.0) * resolution;
    double lon, lat;
    if (to_wgs84_.Transform(centre_x, centre_y, lon, lat)) {
        zoomed.lon = lon;
        zoomed.lat = lat;
    }
    return zoomed;
}

} // namespace app
