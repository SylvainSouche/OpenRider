/// @file
/// Graph series derived from an outing's recorded points: plain recorded
/// values and a predefined list of computed series (vertical speed, grade),
/// each smoothed over a window. Units are display units (km/h, m, bpm, %...).
/// Module-level utilities.
#pragma once

#include <openrider/input.h>

#include <vector>

namespace openrider::sdk {

/// The graphs' x axis: time in minutes, or distance in km.
enum class XAxis { Time, Distance };

/// What a series shows.
enum class SeriesKind {
    Speed,          ///< km/h
    Altitude,       ///< m
    HeartRate,      ///< bpm
    Cadence,        ///< rpm / spm
    Temperature,    ///< degrees C
    VerticalSpeed,  ///< computed: unit and time window below
    Grade,          ///< computed: % over a distance window
};

/// Unit of vertical speed.
enum class VSpeedUnit { MetresPerSecond, MetresPerMinute, MetresPerHour };

/// How a series is drawn.
enum class GraphType { Curve, Histogram };

/// What a graph shows and how.
struct SeriesSpec {
    SeriesKind kind = SeriesKind::Altitude;              ///< the series
    GraphType type = GraphType::Curve;                   ///< curve or histogram
    double vspeed_window_s = 60.0;                       ///< vertical speed window (s): 30, 60 or 300
    VSpeedUnit vspeed_unit = VSpeedUnit::MetresPerHour;  ///< vertical speed unit
    double grade_window_m = 100.0;                       ///< grade window (m): 100 or 1000
};

/// A series ready to plot.
struct Series {
    std::vector<double> x;              ///< x values (histogram: bin edges)
    std::vector<double> y;              ///< NaN where undefined; plotted as gaps
    std::vector<size_t> point_index;    ///< source point of each x (histogram: bin start)
};

/// Display name of a series kind.
const char *SeriesLabel(SeriesKind kind);

/// Display unit of a series, per its spec (e.g. "m/h").
const char *SeriesUnit(const SeriesSpec &spec);

/// x value of each point: elapsed minutes or cumulated km.
std::vector<double> XValues(const std::vector<or_point> &points, XAxis axis);

/// The curve of `spec` along `axis`, one value per point.
Series ComputeSeries(const std::vector<or_point> &points, XAxis axis, const SeriesSpec &spec);

/// Histogram: one value per consecutive period. Vertical speed uses periods
/// of its time window, grade periods of its distance window (altitude change
/// over the period); recorded series are averaged per minute (time axis) or
/// per 100 m (distance axis). x holds the bin edges (n + 1 values) and y
/// repeats the last value, ready for a post-step stairs plot.
Series HistogramSeries(const std::vector<or_point> &points, XAxis axis, const SeriesSpec &spec);

/// Vertical speed in m/s at each point: altitude change over the points
/// within +/- window/2 seconds, divided by the time they span.
std::vector<double> VerticalSpeed(const std::vector<or_point> &points, double window_s);
/// Grade in % at each point: altitude change over the points within
/// +/- window/2 metres of distance, divided by the distance they span.
std::vector<double> Grade(const std::vector<or_point> &points, double window_m);

} // namespace openrider::sdk
