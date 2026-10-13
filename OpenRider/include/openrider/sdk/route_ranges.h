/// @file
/// Where each step of a route lies on its track (module-level utilities):
/// the point range of every RouteStep, for segments plugins to fill in once
/// their network is final (after clean-up, which merges, drops and splits
/// steps).
// @impl 0f87-6ace-72ef-1201
#pragma once

#include <openrider/sdk/overview.h>
#include <openrider/sdk/segments.h>

#include <vector>

namespace openrider::sdk {

/// Sets the point range (RouteStep::first, RouteStep::last) of every step
/// of every route of `network`, on the track of `tracks` with the route's
/// id. Each track is cut, in order, into one stretch per step: the cuts
/// minimise the distance of each point to its step's segment, plus the
/// distance of each cut to the node between the two steps. The route's
/// chain of segments is the plugin's; only the cuts along the track are
/// chosen here. Consecutive steps share their cut point; the first step
/// starts at the track's first point, the last ends at its last point.
void AssignStepRanges(SegmentNetwork &network, const std::vector<OverviewTrack> &tracks);

} // namespace openrider::sdk
