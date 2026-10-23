/// @file
/// The tile grids a layer is published in, as the application uses them:
/// those its plugin describes (ABI 2.1), or the Web Mercator grid of a
/// plain XYZ layer (module-level utilities).
#pragma once

#include <openrider/tiles.h>

#include <string>
#include <vector>

namespace maptiles {

/// One level of a grid (see or_tile_matrix).
struct TileMatrix {
    std::string id;              ///< level identifier for URLs
    double resolution = 0.0;     ///< pixel size, in the grid system's units
    double origin_x = 0.0;       ///< west edge of column 0
    double origin_y = 0.0;       ///< north edge of row 0
    int tile_width = 256;        ///< pixels
    int tile_height = 256;       ///< pixels
    int matrix_width = 1;        ///< columns
    int matrix_height = 1;       ///< rows
    int first_column = 0;        ///< first column holding tiles
    int last_column = 0;         ///< last column holding tiles
    int first_row = 0;           ///< first row holding tiles
    int last_row = 0;            ///< last row holding tiles
};

/// A grid: its system and levels, coarsest first.
struct TileGrid {
    std::string id;                  ///< grid identifier ("" for a plain XYZ layer's grid)
    std::string crs;                 ///< "EPSG:<code>"
    std::vector<TileMatrix> levels;  ///< coarsest first
    int first_usable = 0;            ///< first level served (an XYZ layer's min_zoom)
    bool xyz = false;                ///< addressed by tile_url(z = level, x, y)
};

/// Whether the plugin's table has the ABI 2.1 grid functions.
bool HasGridFunctions(const or_tiles_vtable *tiles_vtable);

/// The grids of an open layer: the plugin's own, or else the Web Mercator
/// grid of its XYZ description (levels 0..max_zoom, usable from min_zoom).
std::vector<TileGrid> GridsOf(const or_tiles_vtable *tiles_vtable, void *layer);

/// Web Mercator XYZ grid of `tile_size` pixels, levels 0..max_zoom.
TileGrid WebMercatorGrid(int min_zoom, int max_zoom, int tile_size);

/// Cache directory name of a layer's grid: the layer id, followed by
/// "@<grid id>" for a grid the plugin describes.
std::string CacheName(const std::string &layer_id, const TileGrid &grid);

} // namespace maptiles
