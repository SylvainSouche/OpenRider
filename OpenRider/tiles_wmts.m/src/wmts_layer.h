/// @file
/// A WMTS map layer: its definition, then the grids its service's
/// capabilities declare.
#pragma once

#include "wmts_capabilities.h"

#include <openrider/tiles.h>

#include <string>
#include <vector>

/// One open layer (a plugin handle).
struct WmtsLayer {
    std::string id;                  ///< layer id
    std::string name;                ///< display name
    std::string attribution;         ///< credit line
    std::string headers;             ///< extra HTTP headers, "Name: value" lines
    std::string capabilities_url;    ///< the service's GetCapabilities
    std::vector<std::string> layers; ///< WMTS layers making up this layer, in order of preference
    std::string kvp_url;             ///< GetTile KVP endpoint
    std::vector<WmtsGrid> grids;     ///< from the capabilities
    std::vector<or_tile_grid> described;  ///< grids as the ABI describes them
    or_tile_layer info{};            ///< the layer as the ABI describes it

    /// The URL of a tile of grid `grid`, level `level`.
    std::string TileUrl(size_t grid, size_t level, int column, int row) const;
};
