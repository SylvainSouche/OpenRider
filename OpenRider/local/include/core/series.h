// Graph series derived from an outing's recorded points: plain recorded
// values and a predefined list of computed series (vertical speed, grade),
// each smoothed over a window. Units are display units (km/h, m, bpm, %...).
#pragma once

#include <openrider/input.h>

#include <vector>

namespace core {

enum class XAxis { Time, Distance };  // time in minutes, distance in km

enum class SeriesKind {
    Speed,          // km/h
    Altitude,       // m
    HeartRate,      // bpm
    Cadence,        // rpm / spm
    Temperature,    // degrees C
    VerticalSpeed,  // computed: unit and time window below
    Grade,          // computed: % over a distance window
};

enum class VSpeedUnit { MetresPerSecond, MetresPerMinute, MetresPerHour };

enum class GraphType { Curve, Histogram };

struct SeriesSpec {
    SeriesKind kind = SeriesKind::Altitude;
    GraphType type = GraphType::Curve;
    double vspeed_window_s = 60.0;           // 30, 60 or 300
    VSpeedUnit vspeed_unit = VSpeedUnit::MetresPerHour;
    double grade_window_m = 100.0;           // 100 or 1000
};

struct Series {
    std::vector<double> x;
    std::vector<double> y;              // NaN where undefined; plotted as gaps
    std::vector<size_t> point_index;    // source point of each x (histogram: bin start)
};

const char *series_label(SeriesKind k);
const char *series_unit(const SeriesSpec &s);

std::vector<double> x_values(const std::vector<or_point> &pts, XAxis axis);
Series compute_series(const std::vector<or_point> &pts, XAxis axis, const SeriesSpec &spec);

// Histogram: one value per consecutive period. Vertical speed uses periods
// of its time window, grade periods of its distance window (altitude change
// over the period); recorded series are averaged per minute (time axis) or
// per 100 m (distance axis). x holds the bin edges (n + 1 values) and y
// repeats the last value, ready for a post-step stairs plot.
Series histogram_series(const std::vector<or_point> &pts, XAxis axis, const SeriesSpec &spec);

// Vertical speed in m/s at each point: altitude change over the points
// within +/- window/2 seconds, divided by the time they span.
std::vector<double> vertical_speed(const std::vector<or_point> &pts, double window_s);
// Grade in % at each point: altitude change over the points within
// +/- window/2 metres of distance, divided by the distance they span.
std::vector<double> grade(const std::vector<or_point> &pts, double window_m);

} // namespace core
