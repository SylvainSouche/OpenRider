/// @file
/// Growing the network with a track's pass (module-level utilities).
#pragma once

#include "matching/matching.h"
#include "matching/network.h"

#include <vector>

namespace matching {

/// A stretch off the network (points first..last of `pass`) that mostly
/// runs within twice the tolerance of existing segments is GPS drift along a
/// known path, not a new path: 80% of its points have a station that close.
bool Alongside(const Network &network, const std::vector<core::XY> &pass, size_t first, size_t last,
               const MatchParams &params);

/// Matches one pass of track `route` into the network: matched points are
/// accumulated into their stations; stretches off the network at least
/// `params.min_new_m` long and not alongside it (Alongside()) become new
/// segments, cutting the segments where they leave and rejoin it. Stations
/// are then averaged and reindexed.
void InsertPass(Network &network, const std::vector<core::XY> &pass, int route, const MatchParams &params);

} // namespace matching
