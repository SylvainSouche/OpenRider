/// @file
/// Rests: the stretches of a track spent at one place (module-level
/// utilities). A rest is an interval, not a per-point state: everything
/// that happens during it -- walking around a picnic, the slow walk between
/// two runs -- is pause. Found from the rolling-window statistics
/// (window_stats.h); a plugin chooses the criteria through RestParams.
// @impl 0f87-6acf-972e-b67d
#pragma once

#include <openrider/input.h>

#include <cstddef>
#include <limits>
#include <vector>

namespace openrider::sdk {

/// What makes a rest. A window qualifies when all its criteria hold; NaN
/// leaves a criterion out. Defaults measured on the user's marked rest
/// places and the library (2 min, 30 m, displacement ratio 0.5: most marked
/// rests found, slow progress mostly left out).
struct RestParams {
    double window_s = 120.0;      ///< window over which a track must stay at the place
    double max_radius_m = 30.0;   ///< its positions within this of their centre; 0: no rests
    /// Net displacement over the window at most this share of the path
    /// walked: wandering on the spot goes nowhere, slow climbing does. A
    /// window with less path than max_radius_m passes (barely moving).
    double max_displacement_ratio = 0.5;
    /// Median recorded speed over the window at most this share of the
    /// outing's median moving speed (the scale of the activity).
    double max_speed_ratio = std::numeric_limits<double>::quiet_NaN();
    double still_speed_ms = 0.3;  ///< recorded speed at or below which a point is stopped
};

/// A rest of a track.
struct Rest {
    size_t first = 0;         ///< first point of the rest
    size_t last = 0;          ///< last point, inclusive
    double lon = 0.0;         ///< centre of its positions, WGS84
    double lat = 0.0;         ///< centre of its positions, WGS84
    double duration_s = 0.0;  ///< time from its first point to its last
};

/// The rests of `points`: every point of a qualifying window (see
/// RestParams) belongs to a rest -- so a long pause with walks around it is
/// one rest -- extended to the stopped points touching it, then trimmed to
/// run from its first stopped point to its last (the walk in and out at
/// speed is the way; a rest without any stopped point is kept whole). A
/// rest stays at its place: it ends where the track gets farther than
/// twice max_radius_m from its centre so far, so that it cannot drift along
/// a slow climb (its extent stays within about four times the radius).
/// Rests shorter than the window are dropped.
std::vector<Rest> RestIntervals(const std::vector<or_point> &points, const RestParams &params);

/// A place where tracks rest: rests of any tracks whose centres lie within
/// a radius of each other.
struct RestPlace {
    double lon = 0.0;  ///< mean of the rests' centres, WGS84
    double lat = 0.0;  ///< mean of the rests' centres, WGS84
    int rests = 0;     ///< rests there
};

/// `rests` (of any tracks) grouped into places: a rest joins the first
/// place whose centre is within `radius_m` of its own.
std::vector<RestPlace> RestPlaces(const std::vector<Rest> &rests, double radius_m);

} // namespace openrider::sdk
