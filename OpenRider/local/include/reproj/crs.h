/// @file
/// A coordinate system: its EPSG code, projection, datum and area of use.
/// Coordinates are always (x, y) = (east, north): longitude first for
/// geographic systems. Systems are declared as text and held by a
/// CrsRegistry.
#pragma once

#include "reproj/datum.h"
#include "reproj/projection.h"

#include <memory>
#include <string>

namespace reproj {

/// A rectangle (x0, y0) - (x1, y1); in degrees for an area of use.
struct Box {
    double x0 = 0.0;  ///< west / left
    double y0 = 0.0;  ///< south / bottom (or top for a raster's extent, see WarpMesh)
    double x1 = 0.0;  ///< east / right
    double y1 = 0.0;  ///< north / top (or bottom)
};

/// A coordinate system.
struct Crs {
    int epsg = 0;                                    ///< EPSG code, 0 = none
    std::string name;                                ///< e.g. "RGF93 v1 / Lambert-93"
    std::string definition;                          ///< its PROJ string
    bool geographic = false;                         ///< degrees (true) or metres
    Box area;                                        ///< area of use, WGS84 degrees
    Datum datum;                                     ///< ellipsoid and shift to WGS 84
    std::shared_ptr<const Projection> projection;    ///< shared, immutable

    /// "EPSG:<code>".
    std::string Id() const { return "EPSG:" + std::to_string(epsg); }
    /// Whether a coordinate system was found.
    bool Valid() const { return projection != nullptr; }
};

} // namespace reproj
