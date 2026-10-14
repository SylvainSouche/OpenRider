/// @file
/// Motion analysis shared by filters and statistics (module-level
/// utilities). The recorded speed decides what "not moving" is: the device
/// measures it (GPS Doppler) and it stays near zero while the positions of
/// a stop still wander. Points without recorded speed fall back on a rule
/// of thumb: the outing's 10th-percentile speed, taken on a moving average
/// of positions, sets the scale of a stop for that activity.
#pragma once

#include <openrider/input.h>

#include <vector>

namespace openrider::sdk {

/// Parameters of the not-moving rule.
struct MotionParams {
    /// Not moving: recorded speed at or below this. Garmin's own moving
    /// time follows the same threshold (measured over the library: median
    /// difference 0.0% in running, hiking, cycling).
    double still_speed_ms = 0.3;
    // Fallback, for points without recorded speed:
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

/// true for points not moving: recorded speed at most still_speed_ms; for a
/// point without recorded speed, inside a stretch whose positions, within
/// the window centred on the point, span less than still_fraction *
/// ref_distance.
std::vector<bool> NotMovingMask(const std::vector<or_point> &points, const MotionParams &params);

} // namespace openrider::sdk
