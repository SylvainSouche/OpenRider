#include "core/series.h"

#include <cmath>
#include <limits>

namespace core {

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// Centred-window difference of altitude over a monotonic coordinate `c`
// (time or distance): for each point, the slope dalt/dc between the first
// and last points within +/- half of the window.
std::vector<double> windowed_slope(const std::vector<or_point> &pts, double half,
                                   double (*coord)(const or_point &))
{
    std::vector<double> out(pts.size(), kNaN);
    size_t lo = 0, hi = 0;
    for (size_t i = 0; i < pts.size(); ++i) {
        const double c = coord(pts[i]);
        if (std::isnan(c))
            continue;
        while (lo < i && (std::isnan(coord(pts[lo])) || coord(pts[lo]) < c - half))
            ++lo;
        if (hi < i)
            hi = i;
        while (hi + 1 < pts.size() && !std::isnan(coord(pts[hi + 1])) &&
               coord(pts[hi + 1]) <= c + half)
            ++hi;
        const double span = coord(pts[hi]) - coord(pts[lo]);
        const double dalt = pts[hi].alt_m - pts[lo].alt_m;
        if (span > 0.0 && !std::isnan(dalt))
            out[i] = dalt / span;
    }
    return out;
}

double time_of(const or_point &p) { return p.t_s; }
double dist_of(const or_point &p) { return p.dist_m; }

} // namespace

const char *series_label(SeriesKind k)
{
    switch (k) {
    case SeriesKind::Speed: return "Speed";
    case SeriesKind::Altitude: return "Altitude";
    case SeriesKind::HeartRate: return "Heart rate";
    case SeriesKind::Cadence: return "Cadence";
    case SeriesKind::Temperature: return "Temperature";
    case SeriesKind::VerticalSpeed: return "Vertical speed";
    case SeriesKind::Grade: return "Grade";
    }
    return "";
}

const char *series_unit(const SeriesSpec &s)
{
    switch (s.kind) {
    case SeriesKind::Speed: return "km/h";
    case SeriesKind::Altitude: return "m";
    case SeriesKind::HeartRate: return "bpm";
    case SeriesKind::Cadence: return "/min";
    case SeriesKind::Temperature: return "°C";
    case SeriesKind::Grade: return "%";
    case SeriesKind::VerticalSpeed:
        switch (s.vspeed_unit) {
        case VSpeedUnit::MetresPerSecond: return "m/s";
        case VSpeedUnit::MetresPerMinute: return "m/min";
        case VSpeedUnit::MetresPerHour: return "m/h";
        }
    }
    return "";
}

std::vector<double> x_values(const std::vector<or_point> &pts, XAxis axis)
{
    std::vector<double> x;
    x.reserve(pts.size());
    for (const or_point &p : pts)
        x.push_back(axis == XAxis::Time ? p.t_s / 60.0 : p.dist_m / 1000.0);
    return x;
}

std::vector<double> vertical_speed(const std::vector<or_point> &pts, double window_s)
{
    return windowed_slope(pts, window_s / 2.0, time_of);
}

std::vector<double> grade(const std::vector<or_point> &pts, double window_m)
{
    std::vector<double> g = windowed_slope(pts, window_m / 2.0, dist_of);
    for (double &v : g)
        v *= 100.0;
    return g;
}

// @impl 0f87-6acd-2d65-fc4a
Series compute_series(const std::vector<or_point> &pts, XAxis axis, const SeriesSpec &spec)
{
    Series s;
    s.x = x_values(pts, axis);
    switch (spec.kind) {
    case SeriesKind::VerticalSpeed: {
        s.y = vertical_speed(pts, spec.vspeed_window_s);
        const double k = spec.vspeed_unit == VSpeedUnit::MetresPerHour     ? 3600.0
                         : spec.vspeed_unit == VSpeedUnit::MetresPerMinute ? 60.0
                                                                           : 1.0;
        for (double &v : s.y)
            v *= k;
        return s;
    }
    case SeriesKind::Grade:
        s.y = grade(pts, spec.grade_window_m);
        return s;
    default:
        break;
    }
    s.y.reserve(pts.size());
    for (const or_point &p : pts) {
        switch (spec.kind) {
        case SeriesKind::Speed: s.y.push_back(p.speed_ms * 3.6); break;
        case SeriesKind::Altitude: s.y.push_back(p.alt_m); break;
        case SeriesKind::HeartRate: s.y.push_back(p.hr); break;
        case SeriesKind::Cadence: s.y.push_back(p.cadence); break;
        case SeriesKind::Temperature: s.y.push_back(p.temp_c); break;
        default: s.y.push_back(kNaN); break;
        }
    }
    return s;
}

// @impl 0f87-6acd-335b-49cd
Series histogram_series(const std::vector<or_point> &pts, XAxis axis, const SeriesSpec &spec)
{
    Series s;
    if (pts.size() < 2)
        return s;
    const bool vspeed = spec.kind == SeriesKind::VerticalSpeed;
    const bool grd = spec.kind == SeriesKind::Grade;
    // The coordinate the periods are measured on, and the period length.
    const bool by_time = vspeed || (!grd && axis == XAxis::Time);
    const double width = vspeed ? spec.vspeed_window_s
                         : grd  ? spec.grade_window_m
                         : by_time ? 60.0 : 100.0;
    auto coord = [&](size_t i) { return by_time ? pts[i].t_s : pts[i].dist_m; };
    const std::vector<double> xs = x_values(pts, axis);
    const Series raw = (vspeed || grd) ? Series{} : compute_series(pts, axis, spec);
    const double unit = spec.vspeed_unit == VSpeedUnit::MetresPerHour     ? 3600.0
                        : spec.vspeed_unit == VSpeedUnit::MetresPerMinute ? 60.0
                                                                          : 1.0;
    size_t i = 0;
    while (i < pts.size() && std::isnan(coord(i)))
        ++i;
    while (i + 1 < pts.size()) {
        const double start = coord(i);
        size_t j = i;
        while (j + 1 < pts.size() && !(coord(j + 1) >= start + width))
            ++j;
        if (j + 1 < pts.size())
            ++j;  // the bin ends on the first point at or past its end
        double v = kNaN;
        if (vspeed || grd) {
            const double span = coord(j) - coord(i);
            const double dalt = pts[j].alt_m - pts[i].alt_m;
            if (span > 0.0 && !std::isnan(dalt))
                v = dalt / span * (vspeed ? unit : 100.0);
        } else {
            double sum = 0.0;
            int n = 0;
            for (size_t k = i; k < j; ++k)
                if (!std::isnan(raw.y[k])) {
                    sum += raw.y[k];
                    ++n;
                }
            if (n > 0)
                v = sum / n;
        }
        s.x.push_back(xs[i]);
        s.y.push_back(v);
        s.point_index.push_back(i);
        i = j;
    }
    if (!s.x.empty()) {
        s.x.push_back(xs[i]);
        s.y.push_back(s.y.back());
        s.point_index.push_back(i);
    }
    return s;
}

} // namespace core
