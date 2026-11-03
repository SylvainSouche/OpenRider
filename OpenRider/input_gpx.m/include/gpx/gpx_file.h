/// @file
/// Reading GPX files (module-level utilities): the track of a GPX 1.0/1.1
/// file as recorded points, without an XML library -- GPX tracks are flat.
// @impl 0f87-6ad7-26b7-05d6
#pragma once

#include <openrider/input.h>

#include <string>
#include <vector>

namespace gpx {

/// A GPX file's track, or its route when it has no track.
struct GpxFile {
    std::string name;              ///< the track's (route's, else the metadata's) name, "" if none
    std::string type;              ///< the track's \<type\>, else the sport of a Suunto app export's name
                                   ///< ("suuntoapp-TrailRunning-..." gives "trail_running"); "" if none
    double start_epoch_s = 0.0;    ///< start, seconds since 1970 UTC: the first point's time, else the
                                   ///< metadata's, else a timestamp in the name; NaN if none
    bool route = false;            ///< read from a \<rte\> (planned, or an activity exported as a route)
    bool timed = false;            ///< some point has a time; if none, every t_s is NaN
    std::vector<or_point> points;  ///< every point, in file order; t_s from the first timed point
};

/// Seconds since 1970 UTC of a timestamp in a name, as exports write them:
/// "2026-10-23T12-00-43Z" (Suunto), "2026-10-23T12:00:43Z",
/// "2026-10-23_12-00-43"; NaN if there is none.
double TimeInName(const std::string &name);

/// Seconds since 1970 UTC of an ISO 8601 time ("2022-11-20T11:06:48Z",
/// fractions and "+hh:mm" offsets accepted; no zone means UTC); NaN if
/// unreadable.
double ParseTime(const std::string &text);

/// The track of GPX `text`: its \<trkpt\>, else its route's \<rtept\> (lat,
/// lon, ele, time, and the heart rate, cadence, speed and temperature of
/// Garmin's and similar extensions, whatever their namespace prefix).
/// Missing values are NaN; distance is cumulated along the positions. Times
/// are never invented: a point without one between timed points gets the
/// time interpolated along the distance, and a file with no time at all
/// keeps NaN times (timed = false). Times never go backwards: a drop of
/// about twelve hours (a 12-hour clock written without AM/PM) adds twelve
/// hours from there on; then a time out of step is dropped: a spike ahead of
/// both its neighbours, else any step back.
GpxFile ParseGpx(const std::string &text);

/// "YYYY-MM-DD HH:MM:SS" in local time of `epoch_s`.
std::string LocalTime(double epoch_s);

} // namespace gpx
