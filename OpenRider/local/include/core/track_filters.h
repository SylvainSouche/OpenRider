/// @file
/// Track filters applied to an outing's points (module-level utilities):
/// the same for the selected outing and for the overview tracks.
// @impl 0f87-6acf-70c8-05c3
#pragma once

#include <openrider/filter.h>
#include <openrider/input.h>

#include <string>
#include <vector>

namespace core {

/// A track filter plugin with the values of its declared parameters.
struct TrackFilter {
    const or_filter_vtable *vtable = nullptr;  ///< the plugin (kind OR_FILTER_TRACK)
    std::vector<double> values;                ///< one per declared parameter
};

/// Applies `filters` in order to `points`: each marks the points to keep,
/// the next sees only those. A filter that fails leaves the points as they
/// were and the others go on.
/// @return the failures, "<label>: <message>" each, "; "-separated ("" if none)
std::string FilterPoints(std::vector<or_point> &points, const std::vector<TrackFilter> &filters);

} // namespace core
