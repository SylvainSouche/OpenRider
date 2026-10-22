/// @file
/// How the map shows the world: the display coordinate system, the scale
/// and the viewport, and the conversions between WGS84, display units and
/// screen pixels.
#pragma once

#include "app/map_view_controller.h"

#include "maptiles/coverage.h"

#include "reproj/crs_registry.h"
#include "reproj/grid_projector.h"

#include <memory>

namespace app {

/// Display units per pixel at `zoom` in `display`: zoom z shows the
/// equator's 40,075 km (360 degrees for a geographic system) over
/// 256 * 2^z pixels, as Web Mercator tiles do.
double ResolutionAtZoom(const reproj::Crs &display, double zoom);

/// The zoom at which `display` shows `resolution` units per pixel.
double ZoomAtResolution(const reproj::Crs &display, double resolution);

/// The map's geometry for one frame: built from the view model, read-only.
/// Screen coordinates are pixels from the viewport's top-left corner, y
/// down; display coordinates are the display system's, y north.
class MapGeometry {
public:
    /// The geometry of `camera` in `display` over a width x height viewport.
    MapGeometry(const reproj::Crs &display, const MapCamera &camera, double width, double height);

    /// The geometry of the view model's current camera and display.
    explicit MapGeometry(const MapViewModel &view_model);

    /// Whether the camera's centre lies within the display system's domain.
    bool Valid() const { return valid_; }

    /// The display system.
    const reproj::Crs &Display() const { return display_; }
    /// Display units per pixel.
    double Resolution() const { return resolution_; }
    /// Viewport width, pixels.
    double Width() const { return width_; }
    /// Viewport height, pixels.
    double Height() const { return height_; }

    /// Display coordinates to screen pixels.
    void DisplayToScreen(double x, double y, double &screen_x, double &screen_y) const;
    /// Screen pixels to display coordinates.
    void ScreenToDisplay(double screen_x, double screen_y, double &x, double &y) const;

    /// WGS84 degrees to screen pixels; fast within the view (interpolated
    /// to a quarter of a pixel, through a grid built on the first call),
    /// exact elsewhere and when zoomed out too far for the grid.
    /// @return false where the display system cannot show the point
    bool ToScreen(double lon, double lat, double &screen_x, double &screen_y) const;

    /// Screen pixels to WGS84 degrees (exact).
    /// @return false outside the display system's domain
    bool ToLonlat(double screen_x, double screen_y, double &lon, double &lat) const;

    /// WGS84 box holding the view (the whole world if it cannot be found).
    const maptiles::GeoBox &ViewBox() const { return view_box_; }

    /// The camera at `zoom` keeping the WGS84 point under screen position
    /// (screen_x, screen_y) where it is.
    MapCamera ZoomedAt(const MapCamera &camera, double zoom, double screen_x, double screen_y) const;

private:
    reproj::Crs display_;                              ///< display system
    reproj::Transformer to_wgs84_;                     ///< display -> WGS84
    double resolution_ = 1.0;                          ///< display units per pixel
    double centre_x_ = 0.0, centre_y_ = 0.0;           ///< view centre, display units
    double width_ = 0.0, height_ = 0.0;                ///< viewport, pixels
    bool valid_ = false;                               ///< the centre could be projected
    maptiles::GeoBox view_box_{-180, -90, 180, 90};    ///< WGS84 box of the view
    reproj::Transformer from_wgs84_;                   ///< WGS84 -> display, exact
    mutable std::shared_ptr<const reproj::GridProjector> to_display_;  ///< WGS84 -> display, built when first needed
};

} // namespace app
