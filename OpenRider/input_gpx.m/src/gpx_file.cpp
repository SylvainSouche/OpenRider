#include "gpx/gpx_file.h"

#include <openrider/sdk/motion.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <limits>

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

GpxFile ParseGpx(const std::string &text)
{
    GpxFile file;
    file.start_epoch_s = kNaN;
    const size_t trk = text.find("<trk");
    if (trk != std::string::npos) {
        const size_t first_point = text.find("<trkseg", trk);
        const size_t head_end = first_point == std::string::npos ? text.size() : first_point;
        file.name = Element(text, trk, head_end, "name");
        file.type = Element(text, trk, head_end, "type");
    }
    if (file.name.empty())
        file.name = Element(text, 0, trk == std::string::npos ? text.size() : trk, "name");
    double previous_lat = kNaN, previous_lon = kNaN, distance = 0.0;
    for (size_t at = text.find("<trkpt", 0); at != std::string::npos; at = text.find("<trkpt", at + 6)) {
        const size_t tag_end = text.find('>', at);
        if (tag_end == std::string::npos)
            break;
        const bool empty = text[tag_end - 1] == '/';
        const size_t end = empty ? tag_end : text.find("</trkpt", tag_end);
        if (end == std::string::npos)
            break;
        or_point p{};
        p.lat = Attribute(text, at, tag_end, "lat");
        p.lon = Attribute(text, at, tag_end, "lon");
        p.alt_m = Number(Element(text, tag_end, end, "ele"));
        const double time = ParseTime(Element(text, tag_end, end, "time"));
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
        if (std::isnan(file.start_epoch_s))
            file.start_epoch_s = time;
        p.t_s = std::isnan(time) || std::isnan(file.start_epoch_s) ? kNaN : time - file.start_epoch_s;
        file.points.push_back(p);
        at = end;
    }
    // Points without time follow the previous one by a second (a file of
    // positions only still reads as a track).
    for (size_t i = 0; i < file.points.size(); ++i)
        if (std::isnan(file.points[i].t_s))
            file.points[i].t_s = i > 0 ? file.points[i - 1].t_s + 1.0 : 0.0;
    return file;
}

} // namespace gpx
