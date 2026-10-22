/// @file
/// Datums and the shift between a datum and WGS 84: seven-parameter
/// Helmert transformation through geocentric coordinates, as PROJ's
/// +towgs84 (module-level utilities). Accurate to the parameters' own
/// 1-6 m for the old national datums: enough for display.
#pragma once

#include "reproj/projection.h"

namespace reproj {

/// A datum: its ellipsoid and, when it is not WGS 84 (or a datum taken
/// as WGS 84, such as ETRS89), the Helmert parameters to WGS 84.
struct Datum {
    Ellipsoid ellipsoid = kWgs84;  ///< the datum's ellipsoid
    bool shifted = false;          ///< whether towgs84 applies
    /// tx, ty, tz (metres), rx, ry, rz (arc-seconds, position vector
    /// convention), s (parts per million).
    double towgs84[7] = {0, 0, 0, 0, 0, 0, 0};
};

/// Geographic coordinates on `datum` (degrees, on its ellipsoid surface)
/// to WGS 84.
void ToWgs84(const Datum &datum, double &lon, double &lat);

/// WGS 84 geographic coordinates to `datum` (the inverse of ToWgs84).
void FromWgs84(const Datum &datum, double &lon, double &lat);

} // namespace reproj
