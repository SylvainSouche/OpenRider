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
/// Start() never waits: it supersedes the running build, which is dropped
/// between two groups, and the worker goes on with the latest request only
/// (requests made while sliding a parameter coalesce). The owner polls Take().
class NetworkBuilder {
public:
    /// Stops a running build.
    ~NetworkBuilder();

    /// Requests the networks of `groups`, superseding any earlier request.
    void Start(std::vector<TrackGroup> groups, const SegmentParams &params);

    /// A build is running or waiting.
    bool Building() const { return building_.load(); }

    /// Takes the networks of the latest request, once built.
    /// @param out replaced by the networks, one per group, when they are ready
    /// @return a finished build was taken
    bool Take(std::vector<SegmentGroup> &out);

    /// Drops pending work and waits for the worker; for shutdown.
    void Stop();

private:
    /// Worker loop: builds the latest request until none is pending.
    void Run();

    /// A request: the groups and the parameters to build them with.
    struct Request {
        std::vector<TrackGroup> groups;  ///< groups to build
        SegmentParams params;            ///< parameters to build them with
    };

    std::thread worker_;                 ///< runs Run() while there is work
    std::atomic<bool> building_{false};  ///< a build is running or waiting
    std::atomic<bool> stop_{false};      ///< asks the worker to quit
    std::atomic<unsigned> generation_{0}; ///< incremented by every Start()
    std::mutex mutex_;                   ///< guards everything below
    bool running_ = false;               ///< the worker thread is in its loop
    bool pending_ = false;               ///< request_ not yet picked up
    Request request_;                    ///< latest request
    std::vector<SegmentGroup> result_;   ///< networks of the latest finished build
    bool result_ready_ = false;          ///< result_ not yet taken
};

} // namespace core
