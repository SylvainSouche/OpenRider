#include "heatmap/geometry.h"

#include <cmath>

namespace heatmap {

namespace {

// Keeps, between points a and b, the farthest point from the chord when
// farther than eps, recursively.
void DouglasPeucker(const std::vector<XY> &l, size_t a, size_t b, double eps, std::vector<bool> &keep)
{
    if (b <= a + 1)
        return;
    const double dx = l[b].x - l[a].x, dy = l[b].y - l[a].y;
    const double len = std::hypot(dx, dy);
    double best = -1.0;
    size_t idx = a;
    for (size_t i = a + 1; i < b; ++i) {
        const double d = len > 0 ? std::fabs(dy * (l[i].x - l[a].x) - dx * (l[i].y - l[a].y)) / len
                                 : Dist(l[i], l[a]);
        if (d > best) {
            best = d;
            idx = i;
        }
    }
    if (best > eps) {
        keep[idx] = true;
        DouglasPeucker(l, a, idx, eps, keep);
        DouglasPeucker(l, idx, b, eps, keep);
    }
}

} // namespace

// Points every `step` metres along a polyline (ends included).
std::vector<XY> Densify(const std::vector<XY> &l, double step)
{
    std::vector<XY> out;
    if (l.empty())
        return out;
    out.push_back(l[0]);
    double carry = 0.0;
    for (size_t i = 1; i < l.size(); ++i) {
        const double d = Dist(l[i - 1], l[i]);
        double s = step - carry;
        while (s <= d) {
            const double t = s / d;
            out.push_back({l[i - 1].x + (l[i].x - l[i - 1].x) * t, l[i - 1].y + (l[i].y - l[i - 1].y) * t});
            s += step;
        }
        carry = d - (s - step);
    }
    if (Dist(out.back(), l.back()) > 1e-6)
        out.push_back(l.back());
    return out;
}

std::vector<XY> Simplify(const std::vector<XY> &l, double eps)
{
    if (l.size() < 3)
        return l;
    std::vector<bool> keep(l.size(), false);
    keep.front() = keep.back() = true;
    DouglasPeucker(l, 0, l.size() - 1, eps, keep);
    std::vector<XY> out;
    for (size_t i = 0; i < l.size(); ++i)
        if (keep[i])
            out.push_back(l[i]);
    return out;
}

} // namespace heatmap
