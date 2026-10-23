/// @file
/// The tile grids of WMTS layers, read from the service's capabilities
/// document (OGC 07-057r7) (module-level utilities).
#pragma once

#include "xml_node.h"

#include <openrider/tiles.h>

#include <string>
#include <vector>

/// A tile grid of the layer: one tile matrix set of one WMTS layer, in one
/// coordinate system.
struct WmtsGrid {
    std::string id;           ///< tile matrix set identifier
    std::string crs;          ///< "EPSG:<code>", or the set's own CRS when it is not EPSG
    std::string layer;        ///< WMTS layer identifier
    std::string style;        ///< its default style
    std::string format;       ///< tile format (MIME type)
    std::string url_template; ///< REST ResourceURL template, "" for KVP requests
    std::vector<std::string> level_ids;  ///< tile matrix identifiers
    std::vector<or_tile_matrix> levels;  ///< coarsest first; ids point into level_ids
};

/// "EPSG:<code>" of a WMTS SupportedCRS ("urn:ogc:def:crs:EPSG::25833",
/// "http://www.opengis.net/def/crs/EPSG/0/2154", "EPSG:3857",
/// "IGNF:LAMB93", "urn:ogc:def:crs:OGC:1.3:CRS84"...); the value itself when
/// it names no known EPSG system.
std::string NormalisedCrs(const std::string &supported_crs);

/// The grids of `layers` (WMTS layer identifiers, in order): each layer's
/// tile matrix sets, one per coordinate system (the first layer listing a
/// system wins), restricted to the layer's TileMatrixSetLimits.
/// @param capabilities the parsed document
/// @param layers the WMTS layers making up the map layer
/// @param grids receives the grids
/// @param kvp_url receives the GetTile KVP endpoint ("" if none declared)
/// @return "" or why the document cannot be used
std::string ReadGrids(const XmlNode &capabilities, const std::vector<std::string> &layers,
                      std::vector<WmtsGrid> &grids, std::string &kvp_url);
