/// @file
/// Clean-up of a segment network, shared by segments plugins (module-level
/// utilities): joining segments through nodes that are no junctions,
/// dropping junction blobs, merging duplicate descriptions of one path.
#pragma once

#include <openrider/sdk/rests.h>
#include <openrider/sdk/segments.h>

namespace openrider::sdk {

/// Settings of CleanUpNetwork().
struct CleanupParams {
    double min_turn_m = 80.0;            ///< shorter back-and-forth along a segment is jitter
    double max_loop_m = 150.0;           ///< shorter loops are junction blobs
    double duplicate_distance_m = 25.0;  ///< segments within this of each other are one path
    std::vector<RestPlace> kept_places;  ///< places whose nodes stay nodes (rest places)
    double kept_radius_m = 30.0;         ///< a node within this of a kept place stays
};

/// Joins the two segments meeting at a node that is no junction: exactly
/// two segments touch it, and both carry the same routes, or every route
/// passing there goes from one to the other (none starts, ends or turns
/// around there). Back-and-forth
/// along a segment shorter than `min_turn_m` is removed first as jitter.
/// Nodes within `kept_radius_m` of one of `kept` (rest places) are never
/// joined through. Renumbers segments and nodes, dropping those no route uses.
void MergeThroughNodes(SegmentNetwork &network, double min_turn_m, const std::vector<RestPlace> &kept = {},
                       double kept_radius_m = 0.0);

/// Removes loops (a segment from a node back to itself) shorter than
/// `max_length_m`, whatever their routes: blobs around a junction.
/// @return a loop was removed
bool DropShortLoops(SegmentNetwork &network, double max_length_m);

/// Merges segments describing the same path: two between the same nodes,
/// the shorter within `max_distance_m` of the longer all along; or one
/// duplicating a chain of two through a node in between (its routes are
/// rerouted through the chain). The busiest absorbs the others.
/// @return something was merged
bool MergeDuplicates(SegmentNetwork &network, double max_distance_m);

/// Rounds of the three above until nothing changes.
void CleanUpNetwork(SegmentNetwork &network, const CleanupParams &params);

} // namespace openrider::sdk
