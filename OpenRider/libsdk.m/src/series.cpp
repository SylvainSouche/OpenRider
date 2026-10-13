#include <openrider/sdk/series.h>

#include <cmath>
#include <limits>

namespace openrider::sdk {

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// Centred-window difference of altitude over a monotonic coordinate `c`
// (time or distance): for each point, the slope dalt/dc between the first
// and last points within +/- half of the window.
std::vector<double> WindowedSlope(const std::vector<or_point> &points, double half,
                                   double (*coord)(const or_point &))
{
    std::vector<double> out(points.size(), kNaN);
    size_t lo = 0, hi = 0;
    for (size_t i = 0; i < points.size(); ++i) {
        const double c = coord(points[i]);
        if (std::isnan(c))
            continue;
        while (lo < i && (std::isnan(coord(points[lo])) || coord(points[lo]) < c - half))
            ++lo;
        if (hi < i)
            hi = i;
        while (hi + 1 < points.size() && !std::isnan(coord(points[hi + 1])) &&
               coord(points[hi + 1]) <= c + half)
            ++hi;
        const double span = coord(points[hi]) - coord(points[lo]);
        const double dalt = points[hi].alt_m - points[lo].alt_m;
        if (span > 0.0 && !std::isnan(dalt))
            out[i] = dalt / span;
    }
    return out;
}

double TimeOf(const or_point &point) { return point.t_s; }
double DistOf(const or_point &point) { return point.dist_m; }

} // namespace

const char *SeriesLabel(SeriesKind kind)
{
    switch (kind) {
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

const char *SeriesUnit(const SeriesSpec &spec)
{
    switch (spec.kind) {
    case SeriesKind::Speed: return "km/h";
    case SeriesKind::Altitude: return "m";
    case SeriesKind::HeartRate: return "bpm";
    case SeriesKind::Cadence: return "/min";
    case SeriesKind::Temperature: return "°C";
    case SeriesKind::Grade: return "%";
    case SeriesKind::VerticalSpeed:
        switch (spec.vspeed_unit) {
        case VSpeedUnit::MetresPerSecond: return "m/s";
        case VSpeedUnit::MetresPerMinute: return "m/min";
        case VSpeedUnit::MetresPerHour: return "m/h";
        }
    }
    return "";
}

std::vector<double> XValues(const std::vector<or_point> &points, XAxis axis)
{
    std::vector<double> x;
    x.reserve(points.size());
    for (const or_point &p : points)
        x.push_back(axis == XAxis::Time ? p.t_s / 60.0 : p.dist_m / 1000.0);
    return x;
}

std::vector<double> VerticalSpeed(const std::vector<or_point> &points, double window_s)
{
    return WindowedSlope(points, window_s / 2.0, TimeOf);
}

std::vector<double> Grade(const std::vector<or_point> &points, double window_m)
{
    std::vector<double> g = WindowedSlope(points, window_m / 2.0, DistOf);
    for (double &v : g)
        v *= 100.0;
    return g;
}

// @impl 0f87-6acd-2d65-fc4a
Series ComputeSeries(const std::vector<or_point> &points, XAxis axis, const SeriesSpec &spec)
{
    Series s;
    s.x = XValues(points, axis);
    switch (spec.kind) {
    case SeriesKind::VerticalSpeed: {
        s.y = VerticalSpeed(points, spec.vspeed_window_s);
        const double k = spec.vspeed_unit == VSpeedUnit::MetresPerHour     ? 3600.0
                         : spec.vspeed_unit == VSpeedUnit::MetresPerMinute ? 60.0
                                                                           : 1.0;
        for (double &v : s.y)
            v *= k;
        return s;
    }
    case SeriesKind::Grade:
        s.y = Grade(points, spec.grade_window_m);
        return s;
    default:
        break;
    }
    s.y.reserve(points.size());
    for (const or_point &p : points) {
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
Series HistogramSeries(const std::vector<or_point> &points, XAxis axis, const SeriesSpec &spec)
{
    Series s;
    if (points.size() < 2)
        return s;
    const bool vspeed = spec.kind == SeriesKind::VerticalSpeed;
    const bool grd = spec.kind == SeriesKind::Grade;
    // The coordinate the periods are measured on, and the period length.
    const bool by_time = vspeed || (!grd && axis == XAxis::Time);
    const double width = vspeed ? spec.vspeed_window_s
                         : grd  ? spec.grade_window_m
                         : by_time ? 60.0 : 100.0;
    auto coord = [&](size_t i) { return by_time ? points[i].t_s : points[i].dist_m; };
    const std::vector<double> xs = XValues(points, axis);
    const Series raw = (vspeed || grd) ? Series{} : ComputeSeries(points, axis, spec);
    const double unit = spec.vspeed_unit == VSpeedUnit::MetresPerHour     ? 3600.0
                        : spec.vspeed_unit == VSpeedUnit::MetresPerMinute ? 60.0
                                                                          : 1.0;
    size_t i = 0;
    while (i < points.size() && std::isnan(coord(i)))
        ++i;
    while (i + 1 < points.size()) {
        const double start = coord(i);
        size_t j = i;
        while (j + 1 < points.size() && !(coord(j + 1) >= start + width))
            ++j;
        if (j + 1 < points.size())
            ++j;  // the bin ends on the first point at or past its end
        double v = kNaN;
        if (vspeed || grd) {
            const double span = coord(j) - coord(i);
            const double dalt = points[j].alt_m - points[i].alt_m;
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

} // namespace openrider::sdk
