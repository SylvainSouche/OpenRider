/// @file
/// Motion analysis shared by filters and statistics (module-level
/// utilities). One rule of thumb does most of the work: the outing's
/// 10th-percentile speed, taken on a moving average, sets the scale of what
/// "not moving" means for that activity.
#pragma once

#include <openrider/input.h>

#include <vector>

namespace core {

/// Parameters of the not-moving rule.
struct MotionParams {
    double ref_distance_m = 50.0;  ///< window = time to cover this at the 10th-percentile speed
    double still_fraction = 0.10;  ///< not moving: less than this fraction of it covered
    double speed_window_s = 30.0;  ///< moving average used for the percentile
};

/// Great-circle distance (m) between two WGS84 positions.
double HaversineM(double lat1, double lon1, double lat2, double lon2);

/// Speed (m/s) averaged over +/- window/2 seconds around each point, from
/// positions when available, else from recorded speed.
std::vector<double> MovingAverageSpeed(const std::vector<or_point> &points, double window_s);

/// `percent`-th percentile (0-100) of the defined (non-NaN) values; NaN if none.
double Percentile(std::vector<double> values, double percent);

/// Window (s) for the not-moving test: reference distance at the 10th-
/// percentile moving-average speed (NaN if that speed is not positive).
double StillWindowS(const std::vector<or_point> &points, const MotionParams &params);

/// true for points inside a not-moving stretch: the positions within the
/// window centred on the point span less than still_fraction * ref_distance.
std::vector<bool> NotMovingMask(const std::vector<or_point> &points, const MotionParams &params);

} // namespace core
