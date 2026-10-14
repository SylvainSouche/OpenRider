#include "matching/track_geometry.h"

#include <algorithm>
#include <cmath>

namespace matching {

using openrider::sdk::Dist;
using openrider::sdk::Length;
using openrider::sdk::XY;

std::vector<XY> Stations(const std::vector<XY> &line, double step)
{
    const double length = Length(line);
    return openrider::sdk::Resampled(line, std::max<size_t>(2, static_cast<size_t>(std::ceil(length / step)) + 1));
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


std::vector<size_t> TurnArounds(const openrider::sdk::PlaneTrack &track, double arm, double close, double spike)
{
    std::vector<size_t> turns;
    const std::vector<XY> &line = track.points;
    size_t spike_end = 0;     // index of the last turn's after-point
    double turn_height = 0.0;
    size_t before = 0, after = 0;
    for (size_t i = 0; i < line.size(); ++i) {
        // The points `arm` metres of path before and after i.
        while (before + 1 < i && track.along[i] - track.along[before + 1] >= arm)
            ++before;
        if (after < i)
            after = i;
        while (after + 1 < line.size() && track.along[after] - track.along[i] < arm)
            ++after;
        if (track.along[i] - track.along[before] < arm || track.along[after] - track.along[i] < arm)
            continue;  // too close to an end
        const XY &a = line[before], &b = line[after];
        if (Dist(a, b) >= close || std::min(Dist(line[i], a), Dist(line[i], b)) <= spike)
            continue;
        const double height = Dist(line[i], {(a.x + b.x) / 2.0, (a.y + b.y) / 2.0});
        if (!turns.empty() && before < spike_end) {
            if (height > turn_height) {
                turns.back() = i;
                turn_height = height;
            }
        } else {
            turns.push_back(i);
            turn_height = height;
        }
        spike_end = after;
    }
    return turns;
}


} // namespace matching
