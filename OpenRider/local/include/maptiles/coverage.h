/// @file
/// Whether a layer covers what is shown: a layer is offered as a background
/// only when the displayed view and the displayed track lie largely within
/// its extent. Thresholds are parameters (provisional defaults).
/// Module-level utilities.
#pragma once

#include <vector>

namespace maptiles {

/// A geographic box.
struct GeoBox {
    double west;   ///< WGS84 degrees
    double south;  ///< WGS84 degrees
    double east;   ///< WGS84 degrees
    double north;  ///< WGS84 degrees
};

/// How much of the view and of the track a layer must cover.
struct CoverageRule {
    double min_view_fraction = 0.80;   ///< of the view's area
    double min_track_fraction = 0.95;  ///< of the track's points
};

/// Fraction (0-1) of `view`'s area inside `extent` (west, south, east, north).
double ViewFractionInside(const GeoBox &view, const double extent[4]);

/// Fraction (0-1) of the points inside `extent`; 1 when there are none.
double PointsFractionInside(const std::vector<double> &lons, const std::vector<double> &lats,
                            const double extent[4]);

/// A layer with `extent` is offered for `view` and the track (`lons`,
/// `lats`, possibly empty) when both lie largely within it per `rule`.
bool LayerOffered(const double extent[4], const GeoBox &view, const std::vector<double> &lons,
                  const std::vector<double> &lats, const CoverageRule &rule = CoverageRule{});

} // namespace maptiles
