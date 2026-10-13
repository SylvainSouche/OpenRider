#include "app/graphs.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace app {

openrider::sdk::Series GraphSeries(const std::vector<or_point> &points, openrider::sdk::XAxis axis,
                         const openrider::sdk::SeriesSpec &spec)
{
    return spec.type == openrider::sdk::GraphType::Histogram ? openrider::sdk::HistogramSeries(points, axis, spec)
                                                   : openrider::sdk::ComputeSeries(points, axis, spec);
}

// @impl 0f87-6ace-37d9-900e
double PassageX(const ComparedRun &run, openrider::sdk::XAxis axis, size_t point)
{
    return axis == openrider::sdk::XAxis::Distance ? run.along_m[std::min(point, run.along_m.size() - 1)] / 1000.0
                                         : run.points[point].t_s / 60.0;
}

size_t PassagePointAt(const ComparedRun &run, openrider::sdk::XAxis axis, double x)
{
    size_t nearest = 0;
    double best = std::numeric_limits<double>::infinity();
    for (size_t point = 0; point < run.points.size(); ++point) {
        const double gap = std::fabs(PassageX(run, axis, point) - x);
        if (gap < best) {
            best = gap;
            nearest = point;
        }
    }
    return nearest;
}

} // namespace app
