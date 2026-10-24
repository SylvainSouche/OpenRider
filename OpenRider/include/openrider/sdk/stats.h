/// @file
/// Outing statistics computed from recorded points (module-level
/// utilities), used after a track filter or when the user prefers computed
/// values to Garmin's.
#pragma once

#include <openrider/input.h>
#include <openrider/sdk/motion.h>

#include <vector>

namespace openrider::sdk {

/// Statistics of an outing computed from its points.
struct ComputedStats {
    double distance_m = 0.0;       ///< along the track
    double duration_s = 0.0;       ///< first to last point; NaN when the points have no times
                                   ///< (a route), and then the other times and speeds are NaN too
    double moving_s = 0.0;         ///< outside not-moving stretches
    double ascent_m = 0.0;         ///< D+, with hysteresis
    double descent_m = 0.0;        ///< D-, with hysteresis
    double avg_speed_ms = 0.0;     ///< distance / duration
    double moving_speed_ms = 0.0;  ///< distance / moving time
    double max_speed_ms = 0.0;     ///< max of the moving-average speed (spike-proof)
    double min_alt_m = 0.0;        ///< lowest altitude
    double max_alt_m = 0.0;        ///< highest altitude
};

/// Altimeter noise ignored for D+/D- (m).
constexpr double kAltitudeHysteresisM = 3.0;

/// Computes the statistics of `points`.
/// @param points the points, possibly filtered
/// @param motion parameters of the not-moving rule
/// @param raw when given, the outing as recorded, before track filters: the
///        moving time is computed on it, since filtering removes jitter but
///        does not change when the user was stopped. Everything else comes
///        from `points`.
ComputedStats ComputeStats(const std::vector<or_point> &points, const MotionParams &motion,
                           const std::vector<or_point> *raw = nullptr);

} // namespace openrider::sdk
