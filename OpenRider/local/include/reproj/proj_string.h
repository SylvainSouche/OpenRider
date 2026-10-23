/// @file
/// Coordinate systems described by PROJ strings ("+proj=lcc +lat_1=49
/// ..."), the form `projinfo -o PROJ` and epsg.io print (module-level
/// utilities). Only the methods implemented here are understood: longlat,
/// merc, webmerc, tmerc, utm, lcc, sterea, somerc, krovak, laea.
#pragma once

#include "reproj/datum.h"
#include "reproj/projection.h"

#include <memory>
#include <string>

namespace reproj {

/// Builds the projection and datum a PROJ string describes. Unknown
/// methods, parameters and ellipsoids, units other than metres, grid
/// shifts and prime meridians other than Greenwich are refused rather than
/// approximated.
/// @return the projection, or nullptr with `error` set
std::shared_ptr<const Projection> ParseProjString(const std::string &definition, Datum &datum, bool &geographic,
                                                  std::string &error);

} // namespace reproj
