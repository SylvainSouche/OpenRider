#include "core/overview.h"

#include "core/motion.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace core {

OverviewTrack ThinTrack(const std::string &id, const std::vector<or_point> &points, double spacing_m)
{
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
        if (!track.lat.empty() && HaversineM(track.lat.back(), track.lon.back(), point.lat, point.lon) < spacing_m)
            continue;
        track.lat.push_back(point.lat);
        track.lon.push_back(point.lon);
    }
    if (last_valid < points.size() && !track.lat.empty() &&
        (track.lat.back() != points[last_valid].lat || track.lon.back() != points[last_valid].lon)) {
        track.lat.push_back(points[last_valid].lat);
        track.lon.push_back(points[last_valid].lon);
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
