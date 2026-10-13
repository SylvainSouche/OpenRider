/// @file
/// Tracing a corridor's skeleton into a graph of edges (module-level
/// utilities). ThinImage() is declared in heatmap.h.
#pragma once

#include "heatmap/geometry.h"

#include <vector>

namespace heatmap {

/// An edge of the skeleton graph.
struct Edge {
    int a = -1;              ///< start node id (-1: open end)
    int b = -1;              ///< end node id (-1: open end)
    std::vector<XY> pts;     ///< its points, metres
    bool removed = false;    ///< pruned or merged away
};

/// Skeleton of one corridor component (cells of `cell` metres) traced into
/// edges, junction to junction; node ids continue from `next_node`.
void TraceComponent(const std::vector<Cell> &cells, double cell, std::vector<Edge> &edges, int &next_node);

/// Removes dead ends shorter than `min_spur`, then joins edges through
/// nodes of degree 2.
void CleanGraph(std::vector<Edge> &edges, double min_spur);

} // namespace heatmap
