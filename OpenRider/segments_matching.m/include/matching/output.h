/// @file
/// The matched network as the application's segment network (module-level
/// utilities).
#pragma once

#include "core/local_projection.h"
#include "core/segments.h"
#include "matching/network.h"
#include "matching/routes.h"

#include <vector>

namespace matching {

/// Every node and edge in WGS84, the edges' lines smoothed and ending on
/// their nodes, with the tracks using them; and the chains as routes.
core::SegmentNetwork ToSegmentNetwork(const Network &network, TrackChains chains,
                                      const std::vector<core::OverviewTrack> &tracks,
                                      const core::LocalProjection &projection);

} // namespace matching
