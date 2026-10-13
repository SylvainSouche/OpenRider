/// @file
/// Overview of all outings: tracks thinned to light polylines for the
/// overview map, plus long date formatting for the outing list
/// (module-level utilities).
#pragma once

#include <openrider/input.h>

#include <string>
#include <vector>

namespace core {

/// A track thinned for the overview map, with its bounding box.
struct OverviewTrack {
    std::string id;                ///< outing id
    std::vector<double> lon;       ///< thinned positions, longitudes
    std::vector<double> lat;       ///< thinned positions, latitudes
    double west = 0;               ///< bounding box, degrees
    double south = 0;              ///< bounding box, degrees
    double east = 0;               ///< bounding box, degrees
    double north = 0;              ///< bounding box, degrees
};

/// Spacing (m) between the kept points of overview tracks.
constexpr double kOverviewSpacingM = 30.0;

/// Thins a track for the overview: keeps a point when it is at least
/// `spacing_m` from the last kept one (first and last points always kept);
/// positions only.
OverviewTrack ThinTrack(const std::string &id, const std::vector<or_point> &points, double spacing_m);

/// At least one point of the track lies inside the box (partly within).
bool TrackInBox(const OverviewTrack &track, double west, double south, double east, double north);

/// Long form of a local start time: "2026-10-12 15:53:03" -> "12 October
/// 2026" (en) / "12 octobre 2026" (fr).
std::string LongDate(const char *start_local, const std::string &lang);

} // namespace core
