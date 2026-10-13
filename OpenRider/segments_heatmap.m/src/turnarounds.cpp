#include "heatmap/turnarounds.h"

#include <algorithm>
#include <cmath>

namespace heatmap {

// Indices of the points where a track turns back on itself: the track a
// little before and a little after (`arm` metres along it) passes within
// `close` of itself while the point stands out from both by more than
// `spike` -- an out-and-back. One index per turn: the farthest point.
// @impl 0f87-6ace-1013-e03e
std::vector<size_t> TurnArounds(const std::vector<XY> &line, double arm, double close, double spike)
{
    std::vector<double> along(line.size(), 0.0);
    for (size_t i = 1; i < line.size(); ++i)
        along[i] = along[i - 1] + Dist(line[i - 1], line[i]);
    std::vector<size_t> turns;
    double turn_height = 0.0;  // of the last turn found, while still in its spike
    size_t spike_end = 0;
    for (size_t i = 1; i + 1 < line.size(); ++i) {
        size_t before = i, after = i;
        while (before > 0 && along[i] - along[before] < arm)
            --before;
        while (after + 1 < line.size() && along[after] - along[i] < arm)
            ++after;
        if (along[i] - along[before] < arm || along[after] - along[i] < arm)
            continue;
        const XY base{(line[before].x + line[after].x) / 2.0, (line[before].y + line[after].y) / 2.0};
        const double height = Dist(line[i], base);
        if (Dist(line[before], line[after]) >= close || std::min(Dist(line[i], line[before]), Dist(line[i], line[after])) <= spike)
            continue;
        if (!turns.empty() && before < spike_end) {
            // Same spike: keep its farthest point.
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

// A hairpin bend looks like a turn-around on a single track: the track
// doubles back with its two legs close together. What tells them apart is
// the other tracks: at a turn-around on a path, tracks passing the spot
// carry straight on; through a hairpin, every one of them doubles back
// too. Turns are grouped by place; where at least three tracks pass and
// nearly all of them double back, the place is a bend and its turns go.
// (A dead end everyone turns round at is lost as a turn, but it ends its
// line and is a node anyway.)
void DropHairpins(const std::vector<std::vector<XY>> &xy, double arm, std::vector<std::vector<size_t>> &turns)
{
    const double radius = 40.0;  // turns this close are one place
    struct Place {
        XY at;
        int count = 0;
    };
    std::vector<Place> places;
    for (size_t t = 0; t < turns.size(); ++t)
        for (size_t i : turns[t]) {
            const XY &q = xy[t][i];
            bool joined = false;
            for (Place &place : places)
                if (Dist(place.at, q) <= radius) {
                    place.at = {(place.at.x * place.count + q.x) / (place.count + 1),
                                (place.at.y * place.count + q.y) / (place.count + 1)};
                    ++place.count;
                    joined = true;
                    break;
                }
            if (!joined)
                places.push_back({q, 1});
        }
    std::vector<XY> bends;
    for (const Place &place : places) {
        int passing = 0, doubling = 0;
        for (const std::vector<XY> &line : xy) {
            // The track's point nearest the place, if it passes there.
            size_t nearest = line.size();
            double best = radius;
            for (size_t i = 0; i < line.size(); ++i)
                if (Dist(line[i], place.at) < best) {
                    best = Dist(line[i], place.at);
                    nearest = i;
                }
            if (nearest == line.size())
                continue;
            ++passing;
            // Doubling back: the track `arm` before and after is much
            // closer than the 2 x arm of a straight passage.
            if (Dist(AlongFrom(line, nearest, -arm), AlongFrom(line, nearest, arm)) < arm)
                ++doubling;
        }
        if (passing >= 3 && doubling >= 0.8 * passing)
            bends.push_back(place.at);
    }
    for (size_t t = 0; t < turns.size(); ++t) {
        std::vector<size_t> kept;
        for (size_t i : turns[t]) {
            bool bend = false;
            for (const XY &at : bends)
                bend = bend || Dist(xy[t][i], at) <= radius;
            if (!bend)
                kept.push_back(i);
        }
        turns[t].swap(kept);
    }
}

} // namespace heatmap
