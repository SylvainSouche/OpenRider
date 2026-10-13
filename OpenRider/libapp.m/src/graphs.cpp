#include "app/graphs.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace app {

core::Series GraphSeries(const std::vector<or_point> &points, core::XAxis axis,
                         const core::SeriesSpec &spec)
{
    return spec.type == core::GraphType::Histogram ? core::HistogramSeries(points, axis, spec)
                                                   : core::ComputeSeries(points, axis, spec);
}

// @impl 0f87-6ace-37d9-900e
double PassageX(const ComparedRun &run, core::XAxis axis, size_t point)
{
    return axis == core::XAxis::Distance ? run.along_m[std::min(point, run.along_m.size() - 1)] / 1000.0
                                         : run.points[point].t_s / 60.0;
}

size_t PassagePointAt(const ComparedRun &run, core::XAxis axis, double x)
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
