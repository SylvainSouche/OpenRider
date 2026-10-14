#include "core/track_filters.h"

#include "core/plugin_thread.h"

namespace core {

std::string FilterPoints(std::vector<or_point> &points, const std::vector<TrackFilter> &filters)
{
    std::string errors;
    for (const TrackFilter &filter : filters) {
        if (!filter.vtable || filter.vtable->kind != OR_FILTER_TRACK || points.empty())
            continue;
        std::vector<unsigned char> keep(points.size(), 1);
        char error[256] = "";
        int rc = 0;
        CallPlugin(filter.vtable, [&] {
            rc = filter.vtable->apply_track(points.data(), points.size(), filter.values.data(), keep.data(), error,
                                            sizeof error);
        });
        if (rc != 0) {
            errors += (errors.empty() ? "" : "; ") + std::string(filter.vtable->label) + ": " + error;
            continue;
        }
        std::vector<or_point> kept;
        for (size_t i = 0; i < points.size(); ++i)
            if (keep[i])
                kept.push_back(points[i]);
        points.swap(kept);
    }
    return errors;
}

} // namespace core
