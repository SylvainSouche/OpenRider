/// @file
/// An outing's recorded points, as analysis works on them.
// @impl 0f87-6acf-6371-f34e
#pragma once

#include <openrider/input.h>

#include <string>
#include <vector>

namespace openrider::sdk {

/// One outing's recorded points, in time order, as the input plugin
/// delivered them (a point without position has NaN latitude and
/// longitude). What analysis works on: never thinned tracks or lines
/// derived from them.
struct RecordedTrack {
    std::string id;                ///< outing id
    std::vector<or_point> points;  ///< the recorded points
};

} // namespace openrider::sdk
