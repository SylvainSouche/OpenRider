// Jitter filter: removes position jitter recorded while not moving, and
// isolated positions implying an impossible speed.
//  - Not moving uses the shared rule (core/motion.h): within each
//    not-moving stretch only its first and last points are kept, so the
//    pause keeps its duration but loses its wandering.
//  - A point whose position implies more than overspeed_factor times the
//    outing's maximum moving-average speed (taken while moving) is dropped.

#include <openrider/filter.h>
#include <openrider/plugin.h>
#include <openrider/sdk/motion.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {

const or_filter_param kParams[] = {
    {"ref_distance_m", "Reference distance (m)", 50.0, 5.0, 1000.0},
    {"still_fraction", "Not moving below (fraction of it)", 0.10, 0.01, 1.0},
    {"speed_window_s", "Speed averaging window (s)", 30.0, 5.0, 600.0},
    {"overspeed_factor", "Drop above (x max speed)", 2.0, 1.1, 10.0},
};

bool HasPos(const or_point &p) { return !std::isnan(p.lat) && !std::isnan(p.lon); }

// @impl 0f87-6acd-33a9-64c0
int JitterApply(const or_point *points, size_t count, const double *values, unsigned char *keep,
                 char *err, size_t errlen)
{
    if (!points || !keep || !values) {
        if (err && errlen)
            std::snprintf(err, errlen, "jitter: missing arguments");
        return 1;
    }
    const std::vector<or_point> pts(points, points + count);
    openrider::sdk::MotionParams mp;
    mp.ref_distance_m = values[0];
    mp.still_fraction = values[1];
    mp.speed_window_s = values[2];
    const double overspeed = values[3];

    for (size_t i = 0; i < count; ++i)
        keep[i] = 1;
    // 1. Not-moving stretches keep only their two ends.
    const std::vector<bool> still = openrider::sdk::NotMovingMask(pts, mp);
    for (size_t i = 1; i + 1 < count; ++i)
        if (still[i] && still[i - 1] && still[i + 1])
            keep[i] = 0;

    // 2. Positions implying more than `overspeed` x the max moving speed.
    const std::vector<double> ma = openrider::sdk::MovingAverageSpeed(pts, mp.speed_window_s);
    double max_ms = 0.0;
    for (size_t i = 0; i < count; ++i)
        if (!still[i] && !std::isnan(ma[i]))
            max_ms = std::max(max_ms, ma[i]);
    if (max_ms > 0.0) {
        size_t prev = count;  // last kept point with a position
        for (size_t i = 0; i < count; ++i) {
            if (!keep[i] || !HasPos(pts[i]))
                continue;
            if (prev < count) {
                const double dt = pts[i].t_s - pts[prev].t_s;
                const double d = openrider::sdk::HaversineM(pts[prev].lat, pts[prev].lon, pts[i].lat, pts[i].lon);
                if (dt > 0.0 && d / dt > overspeed * max_ms) {
                    keep[i] = 0;
                    continue;
                }
            }
            prev = i;
        }
    }
    return 0;
}

const or_filter_vtable kVtable = {sizeof(or_filter_vtable), OR_FILTER_TRACK, "jitter", "Jitter while not moving",
                                  sizeof kParams / sizeof kParams[0], kParams, JitterApply};
const or_plugin_info kInfo = {OR_PLUGIN_API_VERSION, OR_PLUGIN_API_MINOR, OR_PLUGIN_FILTER, OR_PLUGIN_THREAD_SAFE, "jitter", "0.1", &kVtable};

} // namespace

extern "C" __attribute__((visibility("default"))) const or_plugin_info *openrider_plugin(void)
{
    return &kInfo;
}
