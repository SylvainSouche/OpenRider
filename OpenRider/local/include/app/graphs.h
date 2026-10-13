/// @file
/// Graph helpers shared by the outing view and its controller
/// (module-level utilities).
#pragma once

#include "core/series.h"

#include <openrider/input.h>

#include <vector>

namespace app {

/// The series a graph shows: curve or histogram, per its spec.
core::Series GraphSeries(const std::vector<or_point> &points, core::XAxis axis,
                         const core::SeriesSpec &spec);

} // namespace app
