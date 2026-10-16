/// @file
/// Statistics of recorded points over a rolling time window (module-level
/// utilities): the measurements a plugin combines to tell where a track
/// stays at a place, moves slowly, or wanders on the spot.
#pragma once

#include <openrider/input.h>

#include <cstddef>
#include <vector>

namespace openrider::sdk {

/// Statistics of the recorded points in the window centred on one point.
/// Distances in metres (longitude scaled by the cosine of the latitude),
/// speeds in m/s; NaN where the window has nothing to measure.
struct WindowStats {
    size_t first = 0;               ///< first point of the window
    size_t last = 0;                ///< last point of the window, inclusive
    double span_s = 0.0;            ///< time covered: last point's time - first point's
    double radius_m = 0.0;          ///< farthest position from the positions' centre
    double displacement_m = 0.0;    ///< from the first position in the window to the last
    double path_m = 0.0;            ///< along the positions
    double speed_path_m = 0.0;      ///< the recorded speed integrated over the window
    double speed_p10 = 0.0;         ///< 10th percentile of the recorded speeds
    double speed_median = 0.0;      ///< median recorded speed
    double speed_p90 = 0.0;         ///< 90th percentile of the recorded speeds
    double stopped_fraction = 0.0;  ///< share of the window's time recorded at or below `still_speed_ms`
};

/// WindowStats of every point, over the points within `window_s` / 2
/// seconds before and after it.
std::vector<WindowStats> RollingStats(const std::vector<or_point> &points, double window_s,
                                      double still_speed_ms = 0.3);

/// `percent`-th percentile (0-100) of the recorded speeds above
/// `still_speed_ms`: the outing's own scale of moving speed. NaN if none.
double MovingSpeedPercentile(const std::vector<or_point> &points, double percent, double still_speed_ms = 0.3);

} // namespace openrider::sdk
