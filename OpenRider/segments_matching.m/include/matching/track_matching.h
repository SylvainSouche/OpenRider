/// @file
/// Matching a track to the network under construction (module-level
/// utilities): a hidden Markov model solved by dynamic programming.
#pragma once

#include "matching/costs.h"
#include "matching/matching.h"
#include "matching/network.h"
#include "matching/plane_track.h"

#include <vector>

namespace matching {

/// Where a point is: on a segment (edge, station, direction of travel along
/// it), or off the network (edge -1).
struct State {
    int edge = -1;                 ///< edge index, -1 = off the network
    int station = -1;              ///< station on the edge
    int direction = 0;             ///< +1 travelling towards the edge's `to`, -1 towards `from`
    double emission = kOffCost;    ///< cost of the point being there
};

/// The cheapest sequence of states for the points of `track`. Candidates of
/// each point are, on every segment whose line passes within
/// `params.tolerance_m` of it (distance across the line, beside the point)
/// with a direction agreeing with the point's velocity (within
/// `params.max_angle_deg`), its nearest station; plus "off the network".
/// Transitions favour advancing along the same segment by the path walked
/// since the previous point, or passing from the end of a segment to the
/// start of the next through their common node; turning back on a segment
/// costs kTurnCost, entering or leaving the network kSwitchCost.
std::vector<State> MatchTrack(const Network &network, const PlaneTrack &track, const MatchParams &params);

} // namespace matching
