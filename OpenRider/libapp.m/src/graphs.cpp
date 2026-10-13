#include "app/graphs.h"

namespace app {

core::Series GraphSeries(const std::vector<or_point> &points, core::XAxis axis,
                         const core::SeriesSpec &spec)
{
    return spec.type == core::GraphType::Histogram ? core::HistogramSeries(points, axis, spec)
                                                   : core::ComputeSeries(points, axis, spec);
}

} // namespace app
