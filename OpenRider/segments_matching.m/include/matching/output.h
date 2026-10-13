/// @file
/// The matched network as the application's segment network (module-level
/// utilities).
#pragma once

#include "matching/network.h"
#include "matching/routes.h"

#include <openrider/sdk/local_projection.h>
#include <openrider/sdk/segments.h>

#include <vector>

namespace matching {

/// Every node and edge in WGS84, the edges' lines smoothed and ending on
/// their nodes, with the tracks using them; and the chains as routes.
openrider::sdk::SegmentNetwork ToSegmentNetwork(const Network &network, TrackChains chains,
                                      const std::vector<openrider::sdk::OverviewTrack> &tracks,
                                      const openrider::sdk::LocalProjection &projection);

} // namespace matching
