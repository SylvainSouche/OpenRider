/// @file
/// Clean-up of a segment network, shared by segments plugins (module-level
/// utilities): joining segments through nodes that are no junctions,
/// dropping junction blobs, merging duplicate descriptions of one path.
#pragma once

#include "core/segments.h"

namespace core {

/// Settings of CleanUpNetwork().
struct CleanupParams {
    double min_turn_m = 80.0;            ///< shorter back-and-forth along a segment is jitter
    double max_loop_m = 150.0;           ///< shorter loops are junction blobs
    double duplicate_distance_m = 25.0;  ///< segments within this of each other are one path
};

/// Joins the two segments meeting at a node that is no junction: exactly
/// two segments touch it, and both carry the same routes, or every route
/// passing there goes from one to the other (none starts, ends or turns
/// around there). Back-and-forth
/// along a segment shorter than `min_turn_m` is removed first as jitter.
/// Renumbers segments and nodes, dropping those no route uses.
void MergeThroughNodes(SegmentNetwork &network, double min_turn_m);

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

} // namespace core
