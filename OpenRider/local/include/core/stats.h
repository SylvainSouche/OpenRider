// Outing statistics computed from recorded points (used after a track
// filter, or when the user prefers computed values to Garmin's).
#pragma once

#include "core/motion.h"

#include <openrider/input.h>

#include <vector>

namespace core {

struct ComputedStats {
    double distance_m = 0.0;
    double duration_s = 0.0;
    double moving_s = 0.0;
    double ascent_m = 0.0;
    double descent_m = 0.0;
    double avg_speed_ms = 0.0;     // distance / duration
    double moving_speed_ms = 0.0;  // distance / moving time
    double max_speed_ms = 0.0;     // max of the moving-average speed (spike-proof)
    double min_alt_m = 0.0;
    double max_alt_m = 0.0;
};

constexpr double kAltitudeHysteresisM = 3.0;  // altimeter noise ignored for D+/D-

// `raw`, when given, is the outing as recorded, before track filters: the
// moving time is computed on it, since filtering removes jitter but does
// not change when the user was stopped. Everything else comes from `pts`.
ComputedStats compute_stats(const std::vector<or_point> &pts, const MotionParams &motion,
                            const std::vector<or_point> *raw = nullptr);

} // namespace core
