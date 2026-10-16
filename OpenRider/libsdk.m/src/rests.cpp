#include <openrider/sdk/rests.h>

#include <openrider/sdk/motion.h>
#include <openrider/sdk/window_stats.h>

#include <cmath>

namespace openrider::sdk {

std::vector<Rest> RestIntervals(const std::vector<or_point> &points, const RestParams &params)
{
    std::vector<Rest> rests;
    if (points.empty() || !(params.max_radius_m > 0.0) || !(params.window_s > 0.0))
        return rests;
    const std::vector<WindowStats> stats = RollingStats(points, params.window_s, params.still_speed_ms);
    const double outing_median = MovingSpeedPercentile(points, 50.0, params.still_speed_ms);
    auto qualifies = [&](const WindowStats &s) {
        if (s.span_s < 0.9 * params.window_s || !(s.radius_m <= params.max_radius_m))
            return false;
        if (!std::isnan(params.max_displacement_ratio) && s.path_m >= params.max_radius_m &&
            s.displacement_m > params.max_displacement_ratio * s.path_m)
            return false;
        if (!std::isnan(params.max_speed_ratio) && !std::isnan(outing_median) &&
            !(s.speed_median <= params.max_speed_ratio * outing_median))
            return false;
        return true;
    };
    // Every point of a qualifying window rests.
    std::vector<bool> resting(points.size(), false);
    for (const WindowStats &s : stats)
        if (qualifies(s))
            for (size_t k = s.first; k <= s.last; ++k)
                resting[k] = true;
    auto stopped = [&](size_t i) { return !std::isnan(points[i].speed_ms) && points[i].speed_ms <= params.still_speed_ms; };
    auto located = [&](size_t i) { return !std::isnan(points[i].lat) && !std::isnan(points[i].lon); };
    const double reach = 2.0 * params.max_radius_m;
    // A rest under construction: its points and the running centre.
    struct Open {
        size_t first = 0, last = 0;
        double sum_lat = 0.0, sum_lon = 0.0;
        size_t count = 0;
        bool Near(const or_point &p, double reach_m) const
        {
            return count == 0 || HaversineM(sum_lat / count, sum_lon / count, p.lat, p.lon) <= reach_m;
        }
        void Add(size_t i, const or_point &p, bool has_position)
        {
            last = i;
            if (has_position) {
                sum_lat += p.lat;
                sum_lon += p.lon;
                ++count;
            }
        }
    };
    auto close = [&](const Open &open) {
        // From its first stopped point to its last: the walk in and out at
        // speed is the way, not the pause (strolls inside it stay).
        size_t first = open.first, last = open.last;
        while (first < last && !stopped(first))
            ++first;
        while (last > first && !stopped(last))
            --last;
        if (!stopped(first)) {  // no stopped point at all: wandering only
            first = open.first;
            last = open.last;
        }
        Rest rest{first, last, 0.0, 0.0, points[last].t_s - points[first].t_s};
        double sum_lat = 0.0, sum_lon = 0.0;
        size_t count = 0;
        for (size_t k = first; k <= last; ++k)
            if (located(k)) {
                sum_lat += points[k].lat;
                sum_lon += points[k].lon;
                ++count;
            }
        if (count == 0 || rest.duration_s < params.window_s)
            return;
        rest.lat = sum_lat / count;
        rest.lon = sum_lon / count;
        rests.push_back(rest);
    };
    bool in_rest = false;
    Open open;
    for (size_t i = 0; i < points.size(); ++i) {
        // A point belongs to the rest under way while it rests (or is stopped
        // right after it) and stays near its centre.
        const bool member = resting[i] || (in_rest && stopped(i));
        if (in_rest && member && (!located(i) || open.Near(points[i], reach))) {
            open.Add(i, points[i], located(i));
            continue;
        }
        if (in_rest)
            close(open);
        in_rest = resting[i];
        if (in_rest) {
            open = Open{};
            open.first = i;
            // The stopped points just before it are part of the pause.
            while (open.first > 0 && stopped(open.first - 1) && (rests.empty() || open.first - 1 > rests.back().last))
                --open.first;
            for (size_t k = open.first; k <= i; ++k)
                open.Add(k, points[k], located(k));
        }
    }
    if (in_rest)
        close(open);
    return rests;
}

std::vector<RestPlace> RestPlaces(const std::vector<Rest> &rests, double radius_m)
{
    std::vector<RestPlace> places;
    for (const Rest &rest : rests) {
        bool joined = false;
        for (RestPlace &place : places)
            if (HaversineM(place.lat, place.lon, rest.lat, rest.lon) <= radius_m) {
                place.lat = (place.lat * place.rests + rest.lat) / (place.rests + 1);
                place.lon = (place.lon * place.rests + rest.lon) / (place.rests + 1);
                ++place.rests;
                joined = true;
                break;
            }
        if (!joined)
            places.push_back({rest.lon, rest.lat, 1});
    }
    return places;
}

} // namespace openrider::sdk
