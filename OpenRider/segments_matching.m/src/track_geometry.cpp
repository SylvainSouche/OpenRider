#include "matching/track_geometry.h"

#include <algorithm>
#include <cmath>

namespace matching {

using core::Dist;
using core::Length;
using core::XY;

std::vector<XY> Stations(const std::vector<XY> &line, double step)
{
    const double length = Length(line);
    return core::Resampled(line, std::max<size_t>(2, static_cast<size_t>(std::ceil(length / step)) + 1));
}


std::vector<XY> Headings(const std::vector<XY> &line)
{
    std::vector<XY> headings(line.size());
    for (size_t i = 0; i < line.size(); ++i) {
        const XY &a = line[i > 0 ? i - 1 : i];
        const XY &b = line[i + 1 < line.size() ? i + 1 : i];
        headings[i] = {b.x - a.x, b.y - a.y};
    }
    return headings;
}


std::vector<XY> Smoothed(const std::vector<XY> &line, int half)
{
    std::vector<XY> out(line.size());
    for (size_t i = 0; i < line.size(); ++i) {
        const int reach = std::min<int>(half, static_cast<int>(std::min(i, line.size() - 1 - i)));
        XY sum;
        for (int k = -reach; k <= reach; ++k) {
            sum.x += line[static_cast<size_t>(static_cast<int>(i) + k)].x;
            sum.y += line[static_cast<size_t>(static_cast<int>(i) + k)].y;
        }
        out[i] = {sum.x / (2 * reach + 1), sum.y / (2 * reach + 1)};
    }
    return out;
}


std::vector<size_t> TurnArounds(const std::vector<XY> &line, double step, double arm, double close, double spike)
{
    std::vector<size_t> turns;
    const size_t reach = static_cast<size_t>(std::ceil(arm / step));
    size_t spike_end = 0;
    double turn_height = 0.0;
    for (size_t i = reach; i + reach < line.size(); ++i) {
        const XY &before = line[i - reach], &after = line[i + reach];
        if (Dist(before, after) >= close || std::min(Dist(line[i], before), Dist(line[i], after)) <= spike)
            continue;
        const double height = Dist(line[i], {(before.x + after.x) / 2.0, (before.y + after.y) / 2.0});
        if (!turns.empty() && i - reach < spike_end) {
            if (height > turn_height) {
                turns.back() = i;
                turn_height = height;
            }
        } else {
            turns.push_back(i);
            turn_height = height;
        }
        spike_end = i + reach;
    }
    return turns;
}


} // namespace matching
