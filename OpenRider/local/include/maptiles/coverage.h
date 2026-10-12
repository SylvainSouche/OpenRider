// Whether a layer covers what is shown: a layer is offered as a background
// only when the displayed view and the displayed track lie largely within
// its extent. Thresholds are parameters (provisional defaults).
#pragma once

#include <vector>

namespace maptiles {

struct GeoBox {
    double west, south, east, north;  // WGS84 degrees
};

struct CoverageRule {
    double min_view_fraction = 0.80;   // of the view's area
    double min_track_fraction = 0.95;  // of the track's points
};

// Fraction (0-1) of `view`'s area inside `extent` (west, south, east, north).
double view_fraction_inside(const GeoBox &view, const double extent[4]);
// Fraction (0-1) of the points inside `extent`; 1 when there are none.
double points_fraction_inside(const std::vector<double> &lons, const std::vector<double> &lats,
                              const double extent[4]);

bool layer_offered(const double extent[4], const GeoBox &view, const std::vector<double> &lons,
                   const std::vector<double> &lats, const CoverageRule &rule = CoverageRule{});

} // namespace maptiles
