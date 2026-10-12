#include "maptiles/coverage.h"

#include <algorithm>
#include <cmath>

namespace maptiles {

double view_fraction_inside(const GeoBox &view, const double extent[4])
{
    const double w = view.east - view.west, h = view.north - view.south;
    if (w <= 0.0 || h <= 0.0)
        return 0.0;
    const double iw = std::min(view.east, extent[2]) - std::max(view.west, extent[0]);
    const double ih = std::min(view.north, extent[3]) - std::max(view.south, extent[1]);
    if (iw <= 0.0 || ih <= 0.0)
        return 0.0;
    return (iw * ih) / (w * h);
}

double points_fraction_inside(const std::vector<double> &lons, const std::vector<double> &lats,
                              const double extent[4])
{
    size_t n = 0, in = 0;
    for (size_t i = 0; i < lons.size() && i < lats.size(); ++i) {
        if (std::isnan(lons[i]) || std::isnan(lats[i]))
            continue;
        ++n;
        if (lons[i] >= extent[0] && lons[i] <= extent[2] && lats[i] >= extent[1] && lats[i] <= extent[3])
            ++in;
    }
    return n == 0 ? 1.0 : static_cast<double>(in) / static_cast<double>(n);
}

// @impl 0f87-6acd-3542-dba2
bool layer_offered(const double extent[4], const GeoBox &view, const std::vector<double> &lons,
                   const std::vector<double> &lats, const CoverageRule &rule)
{
    return view_fraction_inside(view, extent) >= rule.min_view_fraction &&
           points_fraction_inside(lons, lats, extent) >= rule.min_track_fraction;
}

} // namespace maptiles
