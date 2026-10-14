/// @file
/// The tracks as chains of segments once the network is built
/// (module-level utilities).
#pragma once

#include "matching/matching.h"
#include "matching/network.h"
#include <openrider/sdk/plane_track.h>

#include <openrider/sdk/segments.h>

#include <set>
#include <vector>

namespace matching {

/// Each track's chain of segments, and the tracks using each edge.
struct TrackChains {
    std::vector<openrider::sdk::NetworkRoute> routes;  ///< one per track, in input order
    std::vector<std::set<int>> users;        ///< per edge: tracks using it (input indices)
};

/// Cuts edges where a track turns back mid-edge (matched with `params`);
/// requests on an edge closer than `merge_m` make one cut.
void CutAtTurnArounds(Network &network, const std::vector<openrider::sdk::PlaneTrack> &lines, const MatchParams &params,
                      double merge_m);

/// Cuts every edge passing within `radius_m` of one of `places` at its
/// station nearest the place.
void CutAtPlaces(Network &network, const std::vector<openrider::sdk::XY> &places, double radius_m);

/// Each track's chain: its matched states collapsed into steps (one per
/// edge travelled in one direction).
TrackChains ChainTracks(const Network &network, const std::vector<openrider::sdk::PlaneTrack> &lines,
                        const std::vector<openrider::sdk::RecordedTrack> &tracks, const MatchParams &params);

/// Nodes closer than `distance_m` are one junction (tracks leaving a path a
/// few metres apart each cut it): edges are re-pointed to the first node of
/// each group.
void MergeCloseNodes(Network &network, double distance_m);

/// Gaps in the chains: where a track's GPS left the network for a while,
/// its chain jumps from one edge's end to another's start. The way it went
/// is the shortest path between them through the network, unless that path
/// is much longer than the jump (then it went elsewhere).
void FillChainGaps(const Network &network, TrackChains &chains);

} // namespace matching
