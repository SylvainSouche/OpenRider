/// @file
/// Overview of all outings: tracks thinned to light polylines for the
/// overview map, plus long date formatting for the outing list
/// (module-level utilities).
#pragma once

#include <openrider/input.h>

#include <string>
#include <vector>

namespace openrider::sdk {

/// A track thinned for the overview map, with its bounding box.
struct OverviewTrack {
    std::string id;                ///< outing id
    std::vector<double> lon;       ///< thinned positions, longitudes
    std::vector<double> lat;       ///< thinned positions, latitudes
    std::vector<float> noise_m;    ///< GPS noise (m, 1 sigma) around each kept point; empty if unknown
    double west = 0;               ///< bounding box, degrees
    double south = 0;              ///< bounding box, degrees
    double east = 0;               ///< bounding box, degrees
    double north = 0;              ///< bounding box, degrees
};

/// Spacing (m) between the kept points of overview tracks: fine enough for
/// segment analysis to follow switchbacks.
constexpr double kOverviewSpacingM = 10.0;

/// GPS noise (m, 1 sigma) around each point of a recorded track, from the
/// jitter between consecutive points: the median, over about a minute of
/// points around, of the second difference of positions. White noise of
/// sigma per axis gives second differences of median 2.88 sigma; the shape
/// of the path (a bend, a turn-around) changes them over a few points only,
/// which the median ignores. NaN for points without a position.
std::vector<float> PointNoise(const std::vector<or_point> &points);

/// Thins a track for the overview: keeps a point when it is at least
/// `spacing_m` from the last kept one (first and last points always kept);
/// positions, with the GPS noise around each kept point (PointNoise()).
/// Points recorded while not moving (NotMovingMask()) are dropped but for
/// the first of each stop: their jitter is no part of the way.
OverviewTrack ThinTrack(const std::string &id, const std::vector<or_point> &points, double spacing_m);

/// At least one point of the track lies inside the box (partly within).
bool TrackInBox(const OverviewTrack &track, double west, double south, double east, double north);

} // namespace openrider::sdk
