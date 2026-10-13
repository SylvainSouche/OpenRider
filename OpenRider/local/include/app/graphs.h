/// @file
/// Graph helpers shared by the outing view and its controller
/// (module-level utilities).
#pragma once

#include "app/app_model.h"

#include "core/series.h"

#include <openrider/input.h>

#include <vector>

namespace app {

/// The series a graph shows: curve or histogram, per its spec.
core::Series GraphSeries(const std::vector<or_point> &points, core::XAxis axis,
                         const core::SeriesSpec &spec);

/// Position of a compared passage's point on the comparison graphs' x
/// axis: its position along the segment (km) on the distance axis, the
/// time since its entry into the segment (min) on the time axis.
double PassageX(const ComparedRun &run, core::XAxis axis, size_t point);

/// The point of a compared passage nearest `x` on the comparison graphs' x
/// axis (see PassageX()).
size_t PassagePointAt(const ComparedRun &run, core::XAxis axis, double x);

} // namespace app
