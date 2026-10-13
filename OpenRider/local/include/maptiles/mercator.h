/// @file
/// Web Mercator (EPSG:3857) "world pixel" coordinates: at zoom z the world
/// is tile_size * 2^z pixels wide, x growing east from 180W, y growing south
/// from the top edge (85.0511N). Module-level utilities.
#pragma once

namespace maptiles {

/// Latitude of the top and bottom edges of the Web Mercator world.
constexpr double kMaxLatitude = 85.0511287798;

/// Width (and height) of the world in pixels at `zoom`.
double WorldSize(double zoom, int tile_size);

/// WGS84 degrees to world pixels at `zoom`.
void LonlatToWorld(double lon, double lat, double zoom, int tile_size, double &x, double &y);

/// World pixels at `zoom` to WGS84 degrees.
void WorldToLonlat(double x, double y, double zoom, int tile_size, double &lon, double &lat);

/// Largest integer zoom (within [min_zoom, max_zoom]) at which the box
/// [west..east] x [south..north] fits in width x height pixels.
int FitZoom(double west, double south, double east, double north, double width, double height,
            int tile_size, int min_zoom, int max_zoom);

} // namespace maptiles
