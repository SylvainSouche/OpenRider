/// @file
/// Reading GPX files (module-level utilities): the track of a GPX 1.0/1.1
/// file as recorded points, without an XML library -- GPX tracks are flat.
// @impl 0f87-6ad7-26b7-05d6
#pragma once

#include <openrider/input.h>

#include <string>
#include <vector>

namespace gpx {

/// A GPX file's track.
struct GpxFile {
    std::string name;              ///< the track's (else the file's metadata) name, "" if none
    std::string type;              ///< the track's \<type\>, "" if none
    double start_epoch_s = 0.0;    ///< time of the first point, seconds since 1970 UTC; NaN if none
    std::vector<or_point> points;  ///< every track point, in file order; t_s from the first point
};

/// Seconds since 1970 UTC of an ISO 8601 time ("2022-11-20T11:06:48Z",
/// fractions and "+hh:mm" offsets accepted; no zone means UTC); NaN if
/// unreadable.
double ParseTime(const std::string &text);

/// The track of GPX `text`: its \<trkpt\> (lat, lon, ele, time, and the
/// heart rate, cadence, speed and temperature of Garmin's and similar
/// extensions, whatever their namespace prefix). Missing values are NaN;
/// distance is cumulated along the positions.
GpxFile ParseGpx(const std::string &text);

/// "YYYY-MM-DD HH:MM:SS" in local time of `epoch_s`.
std::string LocalTime(double epoch_s);

} // namespace gpx
