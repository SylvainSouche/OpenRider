#include <openrider/sdk/stats.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace openrider::sdk {

// @impl 0f87-6acd-33a9-9fc2
ComputedStats ComputeStats(const std::vector<or_point> &points, const MotionParams &motion,
                            const std::vector<or_point> *raw)
{
    if (raw && raw != &points && !raw->empty()) {
        ComputedStats s = ComputeStats(points, motion);
        s.moving_s = ComputeStats(*raw, motion).moving_s;
        s.moving_speed_ms = s.moving_s > 0.0 ? s.distance_m / s.moving_s : 0.0;
        return s;
    }
    ComputedStats s;
    if (points.size() < 2)
        return s;
    s.duration_s = points.back().t_s - points.front().t_s;
    const std::vector<bool> still = NotMovingMask(points, motion);
    // A segment is also stopped when its own average speed is below the
    // not-moving speed: after a track filter a pause is just its two ends,
    // minutes apart and a few metres from each other.
    const double window = StillWindowS(points, motion);
    const double still_speed =
        std::isnan(window) ? 0.0 : motion.still_fraction * motion.ref_distance_m / window;
    s.min_alt_m = std::numeric_limits<double>::infinity();
    s.max_alt_m = -std::numeric_limits<double>::infinity();
    double ref_alt = std::numeric_limits<double>::quiet_NaN();  // for the hysteresis
    for (size_t i = 0; i < points.size(); ++i) {
        const or_point &p = points[i];
        if (!std::isnan(p.alt_m)) {
            s.min_alt_m = std::min(s.min_alt_m, p.alt_m);
            s.max_alt_m = std::max(s.max_alt_m, p.alt_m);
            if (std::isnan(ref_alt)) {
                ref_alt = p.alt_m;
            } else if (std::fabs(p.alt_m - ref_alt) >= kAltitudeHysteresisM) {
                (p.alt_m > ref_alt ? s.ascent_m : s.descent_m) += std::fabs(p.alt_m - ref_alt);
                ref_alt = p.alt_m;
            }
        }
        if (i == 0)
            continue;
        const or_point &q = points[i - 1];
        double d = 0.0;
        if (!std::isnan(p.lat) && !std::isnan(p.lon) && !std::isnan(q.lat) && !std::isnan(q.lon))
            d = HaversineM(q.lat, q.lon, p.lat, p.lon);
        else if (!std::isnan(p.dist_m) && !std::isnan(q.dist_m))
            d = std::max(0.0, p.dist_m - q.dist_m);
        s.distance_m += d;
        const double dt = p.t_s - q.t_s;
        // The interval counts as its end point does (as the device counts
        // its moving time); without recorded speed, as the fallback rule.
        const bool stopped = !std::isnan(p.speed_ms) ? still[i]
                                                      : (still[i] && still[i - 1]) || (dt > 0.0 && d / dt < still_speed);
        if (!stopped)
            s.moving_s += dt;
    }
    if (std::isinf(s.min_alt_m)) {
        s.min_alt_m = s.max_alt_m = std::numeric_limits<double>::quiet_NaN();
    }
    s.avg_speed_ms = s.duration_s > 0.0 ? s.distance_m / s.duration_s : 0.0;
    s.moving_speed_ms = s.moving_s > 0.0 ? s.distance_m / s.moving_s : 0.0;
    for (double v : MovingAverageSpeed(points, motion.speed_window_s))
        if (!std::isnan(v))
            s.max_speed_ms = std::max(s.max_speed_ms, v);
    return s;
}

} // namespace openrider::sdk
