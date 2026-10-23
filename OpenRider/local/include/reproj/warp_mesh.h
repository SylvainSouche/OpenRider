/// @file
/// Raster warping as a triangle mesh: the hardware path. A raster (a map
/// tile, an image) covering a rectangle of its own coordinate system is
/// drawn into another system as a grid of textured triangles: the grid's
/// nodes are transformed exactly, the GPU interpolates linearly between
/// them and samples the texture. The grid is refined until that linear
/// interpolation strays less than a tolerance from the exact
/// transformation, so the error stays below a pixel at any scale, with
/// any graphics API (OpenGL through Dear ImGui's draw lists, Metal,
/// Vulkan...) and no shader.
#pragma once

#include "reproj/crs.h"
#include "reproj/transformer.h"

#include <cstdint>
#include <vector>

namespace reproj {

/// A node of the mesh: its position in the target system and its texture
/// coordinates in the raster (u right, v down, 0..1).
struct MeshVertex {
    double x = 0.0;  ///< target x (NaN where the transformation fails)
    double y = 0.0;  ///< target y
    float u = 0.0f;  ///< texture coordinate, 0 at the raster's left edge
    float v = 0.0f;  ///< texture coordinate, 0 at the raster's top edge
};

/// A (cells + 1) x (cells + 1) grid of nodes, row by row from the raster's
/// top edge, and the triangles (three indices each) of the cells whose four
/// nodes all transformed.
struct WarpMesh {
    int cells = 0;                      ///< cells along each side; 0 = empty mesh
    std::vector<MeshVertex> vertices;   ///< (cells + 1)^2 nodes
    std::vector<uint32_t> indices;      ///< two triangles per complete cell
    double max_error = 0.0;             ///< largest interpolation error found, target units
};

/// The mesh drawing a raster whose top-left corner is (extent.x0, extent.y0)
/// and bottom-right corner (extent.x1, extent.y1) in the transformer's
/// source system. The grid doubles from 1 cell per side until the linear
/// interpolation errs by at most `tolerance` (target units) at the cells'
/// edge and diagonal midpoints and centres, or `max_cells` is reached.
WarpMesh BuildWarpMesh(const Transformer &transformer, const Box &extent, double tolerance, int max_cells = 64);

} // namespace reproj
