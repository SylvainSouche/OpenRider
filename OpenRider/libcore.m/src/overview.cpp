#include "core/overview.h"

#include "core/motion.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace core {

OverviewTrack thin_track(const std::string &id, const std::vector<or_point> &pts, double spacing_m)
{
    OverviewTrack t;
    t.id = id;
    t.west = t.south = 1e9;
    t.east = t.north = -1e9;
    size_t last_valid = pts.size();
    for (size_t i = 0; i < pts.size(); ++i) {
        const or_point &p = pts[i];
        if (std::isnan(p.lat) || std::isnan(p.lon))
            continue;
        last_valid = i;
        if (!t.lat.empty() && haversine_m(t.lat.back(), t.lon.back(), p.lat, p.lon) < spacing_m)
            continue;
        t.lat.push_back(p.lat);
        t.lon.push_back(p.lon);
    }
    if (last_valid < pts.size() && !t.lat.empty() &&
        (t.lat.back() != pts[last_valid].lat || t.lon.back() != pts[last_valid].lon)) {
        t.lat.push_back(pts[last_valid].lat);
        t.lon.push_back(pts[last_valid].lon);
    }
    for (size_t i = 0; i < t.lat.size(); ++i) {
        t.west = std::min(t.west, t.lon[i]);
        t.east = std::max(t.east, t.lon[i]);
        t.south = std::min(t.south, t.lat[i]);
        t.north = std::max(t.north, t.lat[i]);
    }
    return t;
}

// @impl 0f87-6acd-5b43-c60a
bool track_in_box(const OverviewTrack &t, double west, double south, double east, double north)
{
    if (t.lat.empty() || t.east < west || t.west > east || t.north < south || t.south > north)
        return false;
    for (size_t i = 0; i < t.lat.size(); ++i)
        if (t.lon[i] >= west && t.lon[i] <= east && t.lat[i] >= south && t.lat[i] <= north)
            return true;
    return false;
}

std::string long_date(const char *start_local, const std::string &lang)
{
    static const char *en[] = {"January", "February", "March",     "April",   "May",      "June",
                               "July",    "August",   "September", "October", "November", "December"};
    static const char *fr[] = {"janvier", "février", "mars",      "avril",   "mai",      "juin",
                               "juillet", "août",    "septembre", "octobre", "novembre", "décembre"};
    int y = 0, m = 0, d = 0;
    if (!start_local || std::sscanf(start_local, "%d-%d-%d", &y, &m, &d) != 3 || m < 1 || m > 12)
        return start_local ? start_local : "";
    char buf[48];
    std::snprintf(buf, sizeof buf, "%d %s %d", d, (lang == "fr" ? fr : en)[m - 1], y);
    return buf;
}

OverviewLoader::~OverviewLoader() { stop(); }

void OverviewLoader::stop()
{
    stop_ = true;
    if (worker_.joinable())
        worker_.join();
    loading_ = false;
}

void OverviewLoader::start(const or_input_vtable *input, void *handle, std::vector<std::string> ids)
{
    if (loading_.load())
        return;
    if (worker_.joinable())
        worker_.join();
    done_ = 0;
    total_ = ids.size();
    loading_ = true;
    worker_ = std::thread(&OverviewLoader::run, this, input, handle, std::move(ids));
}

void OverviewLoader::take(std::vector<OverviewTrack> &out)
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (OverviewTrack &t : pending_)
        out.push_back(std::move(t));
    pending_.clear();
}

void OverviewLoader::run(const or_input_vtable *input, void *handle, std::vector<std::string> ids)
{
    for (const std::string &id : ids) {
        if (stop_.load())
            break;
        std::vector<or_point> pts;
        char err[256] = "";
        auto cb = [](void *user, const or_point *p) -> int {
            static_cast<std::vector<or_point> *>(user)->push_back(*p);
            return 0;
        };
        if (input->load_points(handle, id.c_str(), cb, &pts, err, sizeof err) == 0) {
            OverviewTrack t = thin_track(id, pts, kOverviewSpacingM);
            if (!t.lat.empty()) {
                std::lock_guard<std::mutex> lock(mutex_);
                pending_.push_back(std::move(t));
            }
        }
        ++done_;
    }
    loading_ = false;
}

} // namespace core
