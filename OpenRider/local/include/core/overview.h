// Overview of all outings: every track, thinned to a light polyline, read in
// the background through the input plugin; plus long date formatting for
// the outing list.
#pragma once

#include <openrider/input.h>

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace core {

struct OverviewTrack {
    std::string id;
    std::vector<double> lon, lat;  // thinned positions
    double west = 0, south = 0, east = 0, north = 0;
};

constexpr double kOverviewSpacingM = 30.0;

// Keeps a point when it is at least `spacing_m` from the last kept one
// (first and last points always kept); positions only.
OverviewTrack thin_track(const std::string &id, const std::vector<or_point> &pts, double spacing_m);

// At least one point of the track inside the box (partly within).
bool track_in_box(const OverviewTrack &t, double west, double south, double east, double north);

// "2026-10-12 15:53:03" -> "12 October 2026" (en) / "12 octobre 2026" (fr).
std::string long_date(const char *start_local, const std::string &lang);

class OverviewLoader {
public:
    ~OverviewLoader();
    void start(const or_input_vtable *input, void *handle, std::vector<std::string> ids);
    bool loading() const { return loading_.load(); }
    size_t done() const { return done_.load(); }
    size_t total() const { return total_.load(); }
    void take(std::vector<OverviewTrack> &out);  // appends tracks read so far
    void stop();  // interrupts and waits; call before closing the input

private:
    void run(const or_input_vtable *input, void *handle, std::vector<std::string> ids);
    std::thread worker_;
    std::atomic<bool> loading_{false}, stop_{false};
    std::atomic<size_t> done_{0}, total_{0};
    std::mutex mutex_;
    std::vector<OverviewTrack> pending_;
};

} // namespace core
