/// @file
/// Background building of segment networks, one per group of similar
/// activities.
#pragma once

#include "core/overview.h"
#include "core/segments.h"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace core {

/// Tracks of similar activities, analysed together.
struct TrackGroup {
    std::string sport;                  ///< FIT sport shared by the tracks
    std::vector<OverviewTrack> tracks;  ///< the group's tracks
};

/// The segment network of one group.
struct SegmentGroup {
    std::string sport;       ///< FIT sport of the group
    SegmentNetwork network;  ///< its segments, nodes and routes
};

/// Builds the segment network of each group on a worker thread. A new
/// Start() replaces the previous build; the owner polls Take().
class NetworkBuilder {
public:
    /// Stops a running build.
    ~NetworkBuilder();

    /// Starts building the networks of `groups`, stopping a running build first.
    void Start(std::vector<TrackGroup> groups, const SegmentParams &params);

    /// A build is running.
    bool Building() const { return building_.load(); }

    /// Takes the networks of a finished build.
    /// @param out replaced by the networks, one per group, when a build finished
    /// @return a finished build was taken
    bool Take(std::vector<SegmentGroup> &out);

    /// Interrupts a running build (between groups) and waits for it.
    void Stop();

private:
    /// The build, run on the worker thread.
    void Run(std::vector<TrackGroup> groups, SegmentParams params);

    std::thread worker_;                  ///< runs Run()
    std::atomic<bool> building_{false};  ///< a build is running
    std::atomic<bool> stop_{false};      ///< asks the worker to stop
    std::mutex mutex_;                    ///< guards result_ and result_ready_
    std::vector<SegmentGroup> result_;    ///< networks of the finished build
    bool result_ready_ = false;           ///< result_ not yet taken
};

} // namespace core
