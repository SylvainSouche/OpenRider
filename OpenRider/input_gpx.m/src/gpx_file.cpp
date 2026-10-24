#include "gpx/gpx_file.h"

#include <openrider/sdk/motion.h>

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <limits>
#include <vector>

namespace gpx {

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// The text of the first element named `name` (any namespace prefix) in
// text[from, to); "" if none.
std::string Element(const std::string &text, size_t from, size_t to, const std::string &name)
{
    for (size_t at = text.find('<', from); at != std::string::npos && at < to; at = text.find('<', at + 1)) {
        size_t begin = at + 1;
        if (begin < text.size() && (text[begin] == '/' || text[begin] == '?' || text[begin] == '!'))
            continue;
        size_t end = begin;
        while (end < text.size() && text[end] != '>' && text[end] != ' ' && text[end] != '/' && text[end] != '\t' &&
               text[end] != '\n' && text[end] != '\r')
            ++end;
        std::string tag = text.substr(begin, end - begin);
        const size_t colon = tag.find(':');
        if ((colon == std::string::npos ? tag : tag.substr(colon + 1)) != name)
            continue;
        const size_t open_end = text.find('>', end);
        if (open_end == std::string::npos || open_end >= to || text[open_end - 1] == '/')
            return "";
        const size_t close = text.find("</" + tag, open_end);
        if (close == std::string::npos || close > to)
            return "";
        std::string value = text.substr(open_end + 1, close - open_end - 1);
        // CDATA and surrounding blanks.
        if (value.rfind("<![CDATA[", 0) == 0 && value.size() >= 12)
            value = value.substr(9, value.size() - 12);
        const size_t a = value.find_first_not_of(" \t\r\n"), b = value.find_last_not_of(" \t\r\n");
        return a == std::string::npos ? "" : value.substr(a, b - a + 1);
    }
    return "";
}

// The value of attribute `name` in the tag text[from, to).
double Attribute(const std::string &text, size_t from, size_t to, const char *name)
{
    const std::string key = std::string(" ") + name + "=";
    const size_t at = text.find(key, from);
    if (at == std::string::npos || at >= to)
        return kNaN;
    const size_t quote = at + key.size();
    if (quote >= to || (text[quote] != '"' && text[quote] != '\''))
        return kNaN;
    return std::strtod(text.c_str() + quote + 1, nullptr);
}

double Number(const std::string &value) { return value.empty() ? kNaN : std::strtod(value.c_str(), nullptr); }

} // namespace

double ParseTime(const std::string &text)
{
    int year, month, day, hour, minute;
    double second;
    int consumed = 0;
    if (std::sscanf(text.c_str(), "%d-%d-%dT%d:%d:%lf%n", &year, &month, &day, &hour, &minute, &second, &consumed) < 6)
        return kNaN;
    std::tm tm{};
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = 0;
    double epoch = static_cast<double>(timegm(&tm)) + second;
    const char *zone = text.c_str() + consumed;
    int zh = 0, zm = 0;
    if ((*zone == '+' || *zone == '-') && std::sscanf(zone + 1, "%d:%d", &zh, &zm) >= 1)
        epoch -= (*zone == '+' ? 1 : -1) * (zh * 3600.0 + zm * 60.0);
    return epoch;
}

std::string LocalTime(double epoch_s)
{
    if (std::isnan(epoch_s))
        return "";
    const std::time_t t = static_cast<std::time_t>(std::floor(epoch_s));
    std::tm tm{};
    localtime_r(&t, &tm);
    char text[24];
    std::strftime(text, sizeof text, "%Y-%m-%d %H:%M:%S", &tm);
    return text;
}

double TimeInName(const std::string &name)
{
    // The first "YYYY-MM-DD" followed by 'T', '_' or ' ' and three numbers
    // separated by '-' or ':'.
    for (size_t at = 0; at + 19 <= name.size(); ++at) {
        int year, month, day, hour, minute, second;
        char date_sep, sep1, sep2;
        if (std::sscanf(name.c_str() + at, "%4d-%2d-%2d%c%2d%c%2d%c%2d", &year, &month, &day, &date_sep, &hour, &sep1,
                        &minute, &sep2, &second) != 9)
            continue;
        if ((date_sep != 'T' && date_sep != '_' && date_sep != ' ') || (sep1 != '-' && sep1 != ':') || sep1 != sep2 ||
            month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || minute > 59 || second > 60)
            continue;
        char iso[32];
        std::snprintf(iso, sizeof iso, "%04d-%02d-%02dT%02d:%02d:%02dZ", year, month, day, hour, minute, second);
        return ParseTime(iso);
    }
    return kNaN;
}

// @impl 0f87-6adb-a588-1e2c
GpxFile ParseGpx(const std::string &text)
{
    GpxFile file;
    file.start_epoch_s = kNaN;
    // A track if there is one, else a route.
    file.route = text.find("<trkpt") == std::string::npos && text.find("<rtept") != std::string::npos;
    const char *container = file.route ? "<rte" : "<trk";
    const std::string point_tag = file.route ? "<rtept" : "<trkpt", point_close = file.route ? "</rtept" : "</trkpt";
    const size_t trk = text.find(container);
    if (trk != std::string::npos) {
        const size_t first_point = text.find(file.route ? "<rtept" : "<trkseg", trk);
        const size_t head_end = first_point == std::string::npos ? text.size() : first_point;
        file.name = Element(text, trk, head_end, "name");
        file.type = Element(text, trk, head_end, "type");
    }
    const size_t head_end = trk == std::string::npos ? text.size() : trk;
    if (file.name.empty())
        file.name = Element(text, 0, head_end, "name");
    const double metadata_time = ParseTime(Element(text, 0, head_end, "time"));
    // The Suunto app writes no <type>, but names its exports after the
    // sport: "suuntoapp-TrailRunning-2026-...", read as "trail_running".
    if (file.type.empty() && file.name.rfind("suuntoapp-", 0) == 0) {
        const size_t start = 10, end = file.name.find('-', start);
        for (size_t i = start; i < (end == std::string::npos ? file.name.size() : end); ++i) {
            const char c = file.name[i];
            if (std::isupper(static_cast<unsigned char>(c)) && i > start)
                file.type += '_';
            file.type += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }
    std::vector<double> times;
    double previous_lat = kNaN, previous_lon = kNaN, distance = 0.0;
    // Each search starts after the previous point (`at` is moved to its
    // end): a self-closing point may be followed at once by the next.
    for (size_t at = text.find(point_tag, 0); at != std::string::npos; at = text.find(point_tag, at + 1)) {
        const size_t tag_end = text.find('>', at);
        if (tag_end == std::string::npos)
            break;
        const bool empty = text[tag_end - 1] == '/';
        const size_t end = empty ? tag_end : text.find(point_close, tag_end);
        if (end == std::string::npos)
            break;
        or_point p{};
        p.lat = Attribute(text, at, tag_end, "lat");
        p.lon = Attribute(text, at, tag_end, "lon");
        p.alt_m = Number(Element(text, tag_end, end, "ele"));
        times.push_back(ParseTime(Element(text, tag_end, end, "time")));
        p.hr = Number(Element(text, tag_end, end, "hr"));
        p.cadence = Number(Element(text, tag_end, end, "cad"));
        p.speed_ms = Number(Element(text, tag_end, end, "speed"));
        p.temp_c = Number(Element(text, tag_end, end, "atemp"));
        if (std::isnan(p.temp_c))
            p.temp_c = Number(Element(text, tag_end, end, "temp"));
        if (!std::isnan(p.lat) && !std::isnan(p.lon)) {
            if (!std::isnan(previous_lat))
                distance += openrider::sdk::HaversineM(previous_lat, previous_lon, p.lat, p.lon);
            previous_lat = p.lat;
            previous_lon = p.lon;
        }
        p.dist_m = distance;
        file.points.push_back(p);
        at = end;
    }
    // Times never go backwards. A drop of about twelve hours is a 12-hour
    // clock written without AM/PM (seen in exports: 12:59:55 then 01:00:06):
    // twelve hours are added from there on. Then a time out of step is
    // dropped (the point is untimed, and interpolated below): a spike, ahead
    // of both neighbours while they are in order, else any step back.
    constexpr double kHalfDay = 12 * 3600.0, kSlack = 1800.0;
    double offset = 0.0, previous_time = kNaN;
    for (double &time : times) {
        if (std::isnan(time))
            continue;
        time += offset;
        if (!std::isnan(previous_time) && time < previous_time - kHalfDay + kSlack && time > previous_time - kHalfDay - kSlack) {
            offset += kHalfDay;
            time += kHalfDay;
        }
        previous_time = time;
    }
    std::vector<size_t> timed;
    for (size_t i = 0; i < times.size(); ++i)
        if (!std::isnan(times[i]))
            timed.push_back(i);
    for (size_t k = 1; k + 1 < timed.size(); ++k) {
        const double before = times[timed[k - 1]], here = times[timed[k]], after = times[timed[k + 1]];
        if (!std::isnan(before) && here > after && after >= before)
            times[timed[k]] = kNaN;
    }
    previous_time = kNaN;
    for (double &time : times) {
        if (std::isnan(time))
            continue;
        if (!std::isnan(previous_time) && time < previous_time)
            time = kNaN;
        else
            previous_time = time;
    }
    // Times: from the first timed point. Untimed points between timed ones
    // get a time interpolated along the distance, those before the first or
    // after the last the nearest one's; with no time at all, NaN throughout.
    for (double time : times)
        if (!std::isnan(time)) {
            file.start_epoch_s = time;
            file.timed = true;
            break;
        }
    if (file.timed) {
        size_t previous = times.size();
        for (size_t i = 0; i < times.size(); ++i) {
            if (std::isnan(times[i]))
                continue;
            if (previous < times.size() && i > previous + 1) {
                const double d0 = file.points[previous].dist_m, d1 = file.points[i].dist_m;
                for (size_t k = previous + 1; k < i; ++k) {
                    const double f = d1 > d0 ? (file.points[k].dist_m - d0) / (d1 - d0) : double(k - previous) / double(i - previous);
                    times[k] = times[previous] + f * (times[i] - times[previous]);
                }
            } else if (previous == times.size()) {
                for (size_t k = 0; k < i; ++k)
                    times[k] = times[i];
            }
            previous = i;
        }
        for (size_t k = previous + 1; k < times.size(); ++k)
            times[k] = times[previous];
    }
    for (size_t i = 0; i < file.points.size(); ++i)
        file.points[i].t_s = file.timed ? times[i] - file.start_epoch_s : kNaN;
    if (!file.timed) {
        file.start_epoch_s = metadata_time;
        if (std::isnan(file.start_epoch_s))
            file.start_epoch_s = TimeInName(file.name);
    }
    return file;
}

} // namespace gpx
