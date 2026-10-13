#include "core/overview.h"

#include "core/motion.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace core {

std::vector<float> PointNoise(const std::vector<or_point> &points)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    std::vector<float> noise(points.size(), nan);
    // Valid positions in local metres.
    std::vector<size_t> valid;
    for (size_t i = 0; i < points.size(); ++i)
        if (!std::isnan(points[i].lat) && !std::isnan(points[i].lon))
            valid.push_back(i);
    if (valid.size() < 3)
        return noise;
    const double lat0 = points[valid[0]].lat;
    const double kx = 111320.0 * std::cos(lat0 * M_PI / 180.0), ky = 110540.0;
    // Second difference at each interior valid point.
    std::vector<double> jitter(valid.size(), 0.0);
    for (size_t k = 1; k + 1 < valid.size(); ++k) {
        const or_point &a = points[valid[k - 1]], &b = points[valid[k]], &c = points[valid[k + 1]];
        const double dx = ((a.lon - b.lon) + (c.lon - b.lon)) * kx;
        const double dy = ((a.lat - b.lat) + (c.lat - b.lat)) * ky;
        jitter[k] = std::hypot(dx, dy);
    }
    jitter.front() = jitter[1];
    jitter.back() = jitter[jitter.size() - 2];
    // Median over a window of about a minute at one point per second.
    const size_t half = 30;
    std::vector<double> window;
    for (size_t k = 0; k < valid.size(); ++k) {
        const size_t from = k > half ? k - half : 0, to = std::min(valid.size() - 1, k + half);
        window.assign(jitter.begin() + static_cast<long>(from), jitter.begin() + static_cast<long>(to) + 1);
        std::nth_element(window.begin(), window.begin() + static_cast<long>(window.size() / 2), window.end());
        noise[valid[k]] = static_cast<float>(window[window.size() / 2] / 2.884);
    }
    return noise;
}

OverviewTrack ThinTrack(const std::string &id, const std::vector<or_point> &points, double spacing_m)
{
    const std::vector<float> noise = PointNoise(points);
    // Points recorded while not moving are GPS jitter around one spot: a
    // blob that smears a junction and looks like turn-arounds. Only the
    // first point of each stop is kept.
    const std::vector<bool> still = NotMovingMask(points, MotionParams{});
    OverviewTrack track;
    track.id = id;
    track.west = track.south = 1e9;
    track.east = track.north = -1e9;
    size_t last_valid = points.size();
    for (size_t i = 0; i < points.size(); ++i) {
        const or_point &point = points[i];
        if (std::isnan(point.lat) || std::isnan(point.lon))
            continue;
        last_valid = i;
        if (!still.empty() && still[i] && i > 0 && still[i - 1])
            continue;
        if (!track.lat.empty() && HaversineM(track.lat.back(), track.lon.back(), point.lat, point.lon) < spacing_m)
            continue;
        track.lat.push_back(point.lat);
        track.lon.push_back(point.lon);
        track.noise_m.push_back(noise[i]);
    }
    if (last_valid < points.size() && !track.lat.empty() &&
        (track.lat.back() != points[last_valid].lat || track.lon.back() != points[last_valid].lon)) {
        track.lat.push_back(points[last_valid].lat);
        track.lon.push_back(points[last_valid].lon);
        track.noise_m.push_back(noise[last_valid]);
    }
    for (size_t i = 0; i < track.lat.size(); ++i) {
        track.west = std::min(track.west, track.lon[i]);
        track.east = std::max(track.east, track.lon[i]);
        track.south = std::min(track.south, track.lat[i]);
        track.north = std::max(track.north, track.lat[i]);
    }
    return track;
}

// @impl 0f87-6acd-5b43-c60a
bool TrackInBox(const OverviewTrack &track, double west, double south, double east, double north)
{
    if (track.lat.empty() || track.east < west || track.west > east || track.north < south || track.south > north)
        return false;
    for (size_t i = 0; i < track.lat.size(); ++i)
        if (track.lon[i] >= west && track.lon[i] <= east && track.lat[i] >= south && track.lat[i] <= north)
            return true;
    return false;
}

std::string LongDate(const char *start_local, const std::string &lang)
{
    static const char *months_en[] = {"January", "February", "March",     "April",   "May",      "June",
                               "July",    "August",   "September", "October", "November", "December"};
    static const char *months_fr[] = {"janvier", "février", "mars",      "avril",   "mai",      "juin",
                               "juillet", "août",    "septembre", "octobre", "novembre", "décembre"};
    int year = 0, month = 0, day = 0;
    if (!start_local || std::sscanf(start_local, "%d-%d-%d", &year, &month, &day) != 3 || month < 1 || month > 12)
        return start_local ? start_local : "";
    char text[48];
    std::snprintf(text, sizeof text, "%d %s %d", day, (lang == "fr" ? months_fr : months_en)[month - 1], year);
    return text;
}

} // namespace core
