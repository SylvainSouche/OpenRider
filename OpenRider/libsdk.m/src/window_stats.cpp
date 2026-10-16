#include <openrider/sdk/window_stats.h>

#include <openrider/sdk/motion.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace openrider::sdk {

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kMetresPerDegree = 111320.0;
constexpr double kRad = 3.14159265358979323846 / 180.0;

bool HasPos(const or_point &point) { return !std::isnan(point.lat) && !std::isnan(point.lon); }

// The `percent`-th percentile of `values` (reordered).
double Quantile(std::vector<double> &values, double percent)
{
    if (values.empty())
        return kNaN;
    const size_t k = std::min(values.size() - 1, static_cast<size_t>(percent / 100.0 * values.size()));
    std::nth_element(values.begin(), values.begin() + static_cast<long>(k), values.end());
    return values[k];
}

} // namespace

double MovingSpeedPercentile(const std::vector<or_point> &points, double percent, double still_speed_ms)
{
    std::vector<double> speeds;
    for (const or_point &point : points)
        if (!std::isnan(point.speed_ms) && point.speed_ms > still_speed_ms)
            speeds.push_back(point.speed_ms);
    return Quantile(speeds, percent);
}

std::vector<WindowStats> RollingStats(const std::vector<or_point> &points, double window_s, double still_speed_ms)
{
    const size_t count = points.size();
    std::vector<WindowStats> stats(count);
    // Running sums: path along positions, recorded speed integrated, time
    // stopped; and the located points before/after each index.
    std::vector<double> path(count, 0.0), speed_path(count, 0.0), stopped(count, 0.0);
    std::vector<size_t> next_located(count + 1, count), previous_located(count, count);
    size_t last = count;
    for (size_t i = 0; i < count; ++i) {
        if (i > 0) {
            const or_point &a = points[i - 1], &b = points[i];
            const double dt = std::max(0.0, b.t_s - a.t_s);
            path[i] = path[i - 1];
            if (HasPos(b) && last < count)
                path[i] += HaversineM(points[last].lat, points[last].lon, b.lat, b.lon);
            speed_path[i] = speed_path[i - 1];
            stopped[i] = stopped[i - 1];
            if (!std::isnan(a.speed_ms) && !std::isnan(b.speed_ms)) {
                speed_path[i] += (a.speed_ms + b.speed_ms) / 2.0 * dt;
                if (b.speed_ms <= still_speed_ms)
                    stopped[i] += dt;
            }
        }
        previous_located[i] = HasPos(points[i]) ? i : last;
        if (HasPos(points[i]))
            last = i;
    }
    for (size_t i = count; i-- > 0;)
        next_located[i] = HasPos(points[i]) ? i : next_located[i + 1];

    const double half = window_s / 2.0;
    size_t lo = 0, hi = 0;
    std::vector<double> speeds;
    for (size_t i = 0; i < count; ++i) {
        while (lo < i && points[lo].t_s < points[i].t_s - half)
            ++lo;
        if (hi < i)
            hi = i;
        while (hi + 1 < count && points[hi + 1].t_s <= points[i].t_s + half)
            ++hi;
        WindowStats &s = stats[i];
        s.first = lo;
        s.last = hi;
        s.span_s = points[hi].t_s - points[lo].t_s;
        s.path_m = path[hi] - path[lo];
        s.speed_path_m = speed_path[hi] - speed_path[lo];
        s.stopped_fraction = s.span_s > 0.0 ? (stopped[hi] - stopped[lo]) / s.span_s : kNaN;
        // Positions: centre, farthest from it, first to last.
        double sum_lat = 0.0, sum_lon = 0.0;
        size_t located = 0;
        speeds.clear();
        for (size_t k = lo; k <= hi; ++k) {
            if (HasPos(points[k])) {
                sum_lat += points[k].lat;
                sum_lon += points[k].lon;
                ++located;
            }
            if (!std::isnan(points[k].speed_ms))
                speeds.push_back(points[k].speed_ms);
        }
        if (located > 0) {
            const double lat = sum_lat / located, lon = sum_lon / located;
            const double kx = kMetresPerDegree * std::cos(lat * kRad), ky = 110540.0;
            double radius = 0.0;
            for (size_t k = lo; k <= hi; ++k)
                if (HasPos(points[k]))
                    radius = std::max(radius, std::hypot((points[k].lon - lon) * kx, (points[k].lat - lat) * ky));
            s.radius_m = radius;
            const size_t a = next_located[lo], b = previous_located[hi];
            s.displacement_m = HaversineM(points[a].lat, points[a].lon, points[b].lat, points[b].lon);
        } else {
            s.radius_m = s.displacement_m = kNaN;
        }
        s.speed_p10 = Quantile(speeds, 10.0);
        s.speed_median = Quantile(speeds, 50.0);
        s.speed_p90 = Quantile(speeds, 90.0);
    }
    return stats;
}

} // namespace openrider::sdk
