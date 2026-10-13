/// @file
/// The stages of the heat-map network assembly, in order (module-level
/// utilities). Each works on the shared Assembly.
#pragma once

#include "heatmap/assembly.h"

namespace heatmap {

/// 1. Traversals: each shared piece of a track as monotone runs along its
/// line (a turn-around starts a new run); its own pieces kept as they are.
/// Their entry and exit vertices are cut candidates.
void FindTraversals(Assembly &assembly);

/// 2. Cuts per shared line: its ends plus the entry/exit points, clustered
/// within the cut resolution.
void CutSharedLines(Assembly &assembly);

/// 3. Nodes: line ends meeting at a skeleton junction are one node (union
/// by proximity); interior cuts are nodes of their own.
void PlaceNodes(Assembly &assembly);

/// 4. Shared segments: one proto per range between consecutive cuts.
void MakeSharedSegments(Assembly &assembly);

/// 5. Routes: shared traversals cover whole ranges; own parts become
/// single-route protos between the nodes where the route leaves and rejoins
/// the network; junction crossings and connectors keep chains continuous.
void ChainRoutes(Assembly &assembly);

/// 5a. Bundles: own parts and segments between the same nodes (within the
/// cut resolution) and of similar shape are one path, followed by all of
/// them.
void BundleParts(Assembly &assembly);

/// 5b. Shape of the shared segments: the median of the tracks' runs along
/// each, between its two node positions.
void ShapeSegments(Assembly &assembly);

/// 6a. Segments shorter than the trace width are contracted into a node;
/// short single-route loops dropped.
void ContractShortSegments(Assembly &assembly);

/// 6. Output: final node ids (merged, at medoid positions), segments used
/// by a route (lines ending on their nodes), routes.
void WriteNetwork(Assembly &assembly);

} // namespace heatmap
