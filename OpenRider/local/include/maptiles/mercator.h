// Web Mercator (EPSG:3857) "world pixel" coordinates: at zoom z the world
// is tile_size * 2^z pixels wide, x growing east from 180W, y growing south
// from the top edge (85.0511N).
#pragma once

namespace maptiles {

constexpr double kMaxLatitude = 85.0511287798;

double world_size(double zoom, int tile_size);
void lonlat_to_world(double lon, double lat, double zoom, int tile_size, double &x, double &y);
void world_to_lonlat(double x, double y, double zoom, int tile_size, double &lon, double &lat);

// Largest integer zoom (within [min_zoom, max_zoom]) at which the box
// [west..east] x [south..north] fits in width x height pixels.
int fit_zoom(double west, double south, double east, double north, double width, double height,
             int tile_size, int min_zoom, int max_zoom);

} // namespace maptiles
