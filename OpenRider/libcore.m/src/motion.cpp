#include "core/motion.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace core {

namespace {
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kEarthRadius = 6371000.0;
constexpr double kRad = 3.14159265358979323846 / 180.0;

bool has_pos(const or_point &p) { return !std::isnan(p.lat) && !std::isnan(p.lon); }
} // namespace

double haversine_m(double lat1, double lon1, double lat2, double lon2)
{
    const double p1 = lat1 * kRad, p2 = lat2 * kRad;
    const double dp = p2 - p1, dl = (lon2 - lon1) * kRad;
    const double h = std::sin(dp / 2) * std::sin(dp / 2) +
                     std::cos(p1) * std::cos(p2) * std::sin(dl / 2) * std::sin(dl / 2);
    return 2.0 * kEarthRadius * std::asin(std::min(1.0, std::sqrt(h)));
}

std::vector<double> moving_average_speed(const std::vector<or_point> &pts, double window_s)
{
    // Cumulative distance from positions (recorded distance as fallback).
    std::vector<double> cum(pts.size(), 0.0);
    for (size_t i = 1; i < pts.size(); ++i) {
        double d = 0.0;
        if (has_pos(pts[i]) && has_pos(pts[i - 1]))
            d = haversine_m(pts[i - 1].lat, pts[i - 1].lon, pts[i].lat, pts[i].lon);
        else if (!std::isnan(pts[i].dist_m) && !std::isnan(pts[i - 1].dist_m))
            d = std::max(0.0, pts[i].dist_m - pts[i - 1].dist_m);
        cum[i] = cum[i - 1] + d;
    }
    std::vector<double> out(pts.size(), kNaN);
    const double half = window_s / 2.0;
    size_t lo = 0, hi = 0;
    for (size_t i = 0; i < pts.size(); ++i) {
        while (lo < i && pts[lo].t_s < pts[i].t_s - half)
            ++lo;
        if (hi < i)
            hi = i;
        while (hi + 1 < pts.size() && pts[hi + 1].t_s <= pts[i].t_s + half)
            ++hi;
        const double dt = pts[hi].t_s - pts[lo].t_s;
        if (dt > 0.0)
            out[i] = (cum[hi] - cum[lo]) / dt;
    }
    return out;
}

double percentile(std::vector<double> values, double p)
{
    values.erase(std::remove_if(values.begin(), values.end(), [](double v) { return std::isnan(v); }),
                 values.end());
    if (values.empty())
        return kNaN;
    std::sort(values.begin(), values.end());
    const size_t k = std::min(values.size() - 1, static_cast<size_t>(p / 100.0 * values.size()));
    return values[k];
}

// @impl 0f87-6acd-33a9-4f41
double still_window_s(const std::vector<or_point> &pts, const MotionParams &p)
{
    const double p10 = percentile(moving_average_speed(pts, p.speed_window_s), 10.0);
    return p10 > 0.0 ? p.ref_distance_m / p10 : kNaN;
}

std::vector<bool> not_moving_mask(const std::vector<or_point> &pts, const MotionParams &p)
{
    std::vector<bool> still(pts.size(), false);
    const double window = still_window_s(pts, p);
    if (std::isnan(window))
        return still;
    const double half = window / 2.0;
    const double limit = p.still_fraction * p.ref_distance_m;
    size_t lo = 0, hi = 0;
    for (size_t i = 0; i < pts.size(); ++i) {
        while (lo < i && pts[lo].t_s < pts[i].t_s - half)
            ++lo;
        if (hi < i)
            hi = i;
        while (hi + 1 < pts.size() && pts[hi + 1].t_s <= pts[i].t_s + half)
            ++hi;
        if (hi == lo || !has_pos(pts[i]))
            continue;
        // Largest distance from this point to any position in the window:
        // a stretch is still when everything stays within the limit.
        double span = 0.0;
        for (size_t k = lo; k <= hi && span < limit; ++k)
            if (has_pos(pts[k]))
                span = std::max(span, haversine_m(pts[i].lat, pts[i].lon, pts[k].lat, pts[k].lon));
        still[i] = span < limit && pts[hi].t_s - pts[lo].t_s >= window * 0.5;
    }
    // A centred window at the edge of a pause still reaches into the
    // movement around it, so edge points escape the test above. Extend each
    // still point to the points of its window lying within the same radius:
    // the pause's own jitter joins it, the movement around it does not.
    std::vector<bool> out = still;
    lo = 0;
    hi = 0;
    for (size_t i = 0; i < pts.size(); ++i) {
        while (lo < i && pts[lo].t_s < pts[i].t_s - half)
            ++lo;
        if (hi < i)
            hi = i;
        while (hi + 1 < pts.size() && pts[hi + 1].t_s <= pts[i].t_s + half)
            ++hi;
        if (!still[i])
            continue;
        for (size_t k = lo; k <= hi; ++k)
            if (!out[k] && has_pos(pts[k]) &&
                haversine_m(pts[i].lat, pts[i].lon, pts[k].lat, pts[k].lon) < limit)
                out[k] = true;
    }
    return out;
}

} // namespace core
