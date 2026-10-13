#include <openrider/sdk/motion.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace openrider::sdk {

namespace {
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kEarthRadius = 6371000.0;
constexpr double kRad = 3.14159265358979323846 / 180.0;

bool HasPos(const or_point &point) { return !std::isnan(point.lat) && !std::isnan(point.lon); }
} // namespace

double HaversineM(double lat1, double lon1, double lat2, double lon2)
{
    const double p1 = lat1 * kRad, p2 = lat2 * kRad;
    const double dp = p2 - p1, dl = (lon2 - lon1) * kRad;
    const double h = std::sin(dp / 2) * std::sin(dp / 2) +
                     std::cos(p1) * std::cos(p2) * std::sin(dl / 2) * std::sin(dl / 2);
    return 2.0 * kEarthRadius * std::asin(std::min(1.0, std::sqrt(h)));
}

std::vector<double> MovingAverageSpeed(const std::vector<or_point> &points, double window_s)
{
    // Cumulative distance from positions (recorded distance as fallback).
    std::vector<double> cum(points.size(), 0.0);
    for (size_t i = 1; i < points.size(); ++i) {
        double d = 0.0;
        if (HasPos(points[i]) && HasPos(points[i - 1]))
            d = HaversineM(points[i - 1].lat, points[i - 1].lon, points[i].lat, points[i].lon);
        else if (!std::isnan(points[i].dist_m) && !std::isnan(points[i - 1].dist_m))
            d = std::max(0.0, points[i].dist_m - points[i - 1].dist_m);
        cum[i] = cum[i - 1] + d;
    }
    std::vector<double> out(points.size(), kNaN);
    const double half = window_s / 2.0;
    size_t lo = 0, hi = 0;
    for (size_t i = 0; i < points.size(); ++i) {
        while (lo < i && points[lo].t_s < points[i].t_s - half)
            ++lo;
        if (hi < i)
            hi = i;
        while (hi + 1 < points.size() && points[hi + 1].t_s <= points[i].t_s + half)
            ++hi;
        const double dt = points[hi].t_s - points[lo].t_s;
        if (dt > 0.0)
            out[i] = (cum[hi] - cum[lo]) / dt;
    }
    return out;
}

double Percentile(std::vector<double> values, double percent)
{
    values.erase(std::remove_if(values.begin(), values.end(), [](double v) { return std::isnan(v); }),
                 values.end());
    if (values.empty())
        return kNaN;
    std::sort(values.begin(), values.end());
    const size_t k = std::min(values.size() - 1, static_cast<size_t>(percent / 100.0 * values.size()));
    return values[k];
}

// @impl 0f87-6acd-33a9-4f41
double StillWindowS(const std::vector<or_point> &points, const MotionParams &params)
{
    const double p10 = Percentile(MovingAverageSpeed(points, params.speed_window_s), 10.0);
    return p10 > 0.0 ? params.ref_distance_m / p10 : kNaN;
}

std::vector<bool> NotMovingMask(const std::vector<or_point> &points, const MotionParams &params)
{
    std::vector<bool> still(points.size(), false);
    const double window = StillWindowS(points, params);
    if (std::isnan(window))
        return still;
    const double half = window / 2.0;
    const double limit = params.still_fraction * params.ref_distance_m;
    size_t lo = 0, hi = 0;
    for (size_t i = 0; i < points.size(); ++i) {
        while (lo < i && points[lo].t_s < points[i].t_s - half)
            ++lo;
        if (hi < i)
            hi = i;
        while (hi + 1 < points.size() && points[hi + 1].t_s <= points[i].t_s + half)
            ++hi;
        if (hi == lo || !HasPos(points[i]))
            continue;
        // Largest distance from this point to any position in the window:
        // a stretch is still when everything stays within the limit.
        double span = 0.0;
        for (size_t k = lo; k <= hi && span < limit; ++k)
            if (HasPos(points[k]))
                span = std::max(span, HaversineM(points[i].lat, points[i].lon, points[k].lat, points[k].lon));
        still[i] = span < limit && points[hi].t_s - points[lo].t_s >= window * 0.5;
    }
    // A centred window at the edge of a pause still reaches into the
    // movement around it, so edge points escape the test above. Extend each
    // still point to the points of its window lying within the same radius:
    // the pause's own jitter joins it, the movement around it does not.
    std::vector<bool> out = still;
    lo = 0;
    hi = 0;
    for (size_t i = 0; i < points.size(); ++i) {
        while (lo < i && points[lo].t_s < points[i].t_s - half)
            ++lo;
        if (hi < i)
            hi = i;
        while (hi + 1 < points.size() && points[hi + 1].t_s <= points[i].t_s + half)
            ++hi;
        if (!still[i])
            continue;
        for (size_t k = lo; k <= hi; ++k)
            if (!out[k] && HasPos(points[k]) &&
                HaversineM(points[i].lat, points[i].lon, points[k].lat, points[k].lon) < limit)
                out[k] = true;
    }
    return out;
}

} // namespace openrider::sdk
