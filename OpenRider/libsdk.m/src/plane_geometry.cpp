#include <openrider/sdk/plane_geometry.h>

#include <algorithm>
#include <cmath>

namespace openrider::sdk {

double Dist(const XY &a, const XY &b) { return std::hypot(a.x - b.x, a.y - b.y); }

double Length(const std::vector<XY> &line)
{
    double length = 0.0;
    for (size_t i = 1; i < line.size(); ++i)
        length += Dist(line[i - 1], line[i]);
    return length;
}

XY AlongFrom(const std::vector<XY> &line, size_t from, double distance)
{
    size_t i = from;
    double left = std::fabs(distance);
    if (distance >= 0.0) {
        while (i + 1 < line.size() && left > Dist(line[i], line[i + 1]))
            left -= Dist(line[i], line[i + 1]), ++i;
        if (i + 1 >= line.size())
            return line.back();
        const double step = Dist(line[i], line[i + 1]);
        const double fraction = step > 0.0 ? left / step : 0.0;
        return {line[i].x + (line[i + 1].x - line[i].x) * fraction, line[i].y + (line[i + 1].y - line[i].y) * fraction};
    }
    while (i > 0 && left > Dist(line[i], line[i - 1]))
        left -= Dist(line[i], line[i - 1]), --i;
    if (i == 0)
        return line.front();
    const double step = Dist(line[i], line[i - 1]);
    const double fraction = step > 0.0 ? left / step : 0.0;
    return {line[i].x + (line[i - 1].x - line[i].x) * fraction, line[i].y + (line[i - 1].y - line[i].y) * fraction};
}

double DistanceToLine(const XY &q, const std::vector<XY> &line)
{
    double best = line.empty() ? 0.0 : Dist(q, line.front());
    for (size_t i = 1; i < line.size(); ++i) {
        const double dx = line[i].x - line[i - 1].x, dy = line[i].y - line[i - 1].y;
        const double length2 = dx * dx + dy * dy;
        double along = length2 > 0.0 ? ((q.x - line[i - 1].x) * dx + (q.y - line[i - 1].y) * dy) / length2 : 0.0;
        along = std::clamp(along, 0.0, 1.0);
        best = std::min(best, std::hypot(line[i - 1].x + along * dx - q.x, line[i - 1].y + along * dy - q.y));
    }
    return best;
}

std::vector<XY> Resampled(const std::vector<XY> &line, size_t samples)
{
    std::vector<XY> out;
    if (line.empty() || samples == 0)
        return out;
    const double length = Length(line);
    // Walk the line once, emitting a point every length / (samples - 1).
    const double step = samples > 1 ? length / static_cast<double>(samples - 1) : 0.0;
    size_t i = 0;
    double walked = 0.0;  // along the line up to line[i]
    for (size_t j = 0; j < samples; ++j) {
        const double target = step * static_cast<double>(j);
        while (i + 1 < line.size() && walked + Dist(line[i], line[i + 1]) < target)
            walked += Dist(line[i], line[i + 1]), ++i;
        if (i + 1 >= line.size()) {
            out.push_back(line.back());
            continue;
        }
        const double segment = Dist(line[i], line[i + 1]);
        const double fraction = segment > 0.0 ? (target - walked) / segment : 0.0;
        out.push_back({line[i].x + (line[i + 1].x - line[i].x) * fraction, line[i].y + (line[i + 1].y - line[i].y) * fraction});
    }
    return out;
}

} // namespace openrider::sdk
