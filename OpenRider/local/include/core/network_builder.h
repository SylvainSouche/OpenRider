/// @file
/// Background building of segment networks, one per group of similar
/// activities.
#pragma once

#include <openrider/input.h>
#include <openrider/sdk/recorded_track.h>
#include <openrider/sdk/segments.h>
#include <openrider/segments.h>

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace core {

/// Outings of similar activities, analysed together.
struct TrackGroup {
    std::string sport;             ///< FIT sport shared by the outings
    std::vector<std::string> ids;  ///< the group's outings
};

/// The segment network of one group.
struct SegmentGroup {
    std::string sport;       ///< FIT sport of the group
    openrider::sdk::SegmentNetwork network;  ///< its segments, nodes and routes
};

/// Builds the segment network of each group with a segments plugin on a
/// worker thread, from the outings' recorded points (loaded by the worker
/// through the input plugin, and kept for the next build while their outings
/// are still asked for; outings without any position are left out). A new
/// Start() never waits: it supersedes the running build, which is dropped
/// between two groups, and the worker goes on with the latest request only
/// (requests made while sliding a parameter coalesce). The owner polls Take().
class NetworkBuilder {
public:
    /// Stops a running build.
    ~NetworkBuilder();

    /// Requests the networks of `groups`, superseding any earlier request.
    /// @param groups outings grouped by similar activity, one network each
    ///        (a group with fewer than two outings with a position has none)
    /// @param plugin the segments plugin to use; must outlive the builder
    /// @param values one value per parameter the plugin declares
    /// @param input the input plugin reading the outings' points, and
    /// @param handle its open handle; both must outlive the builder
    void Start(std::vector<TrackGroup> groups, const or_segments_vtable *plugin, std::vector<double> values,
               const or_input_vtable *input, void *handle);

    /// A build is running or waiting.
    bool Building() const { return building_.load(); }

    /// Error of the last finished build, "" if none.
    std::string Error();

    /// Takes the networks of the latest request, once built.
    /// @param out replaced by the networks, one per group, when they are ready
    /// @return a finished build was taken
    bool Take(std::vector<SegmentGroup> &out);

    /// Drops pending work and waits for the worker; for shutdown.
    void Stop();

private:
    /// Worker loop: builds the latest request until none is pending.
    void Run();

    /// A request: the groups, and the plugin and parameter values to build
    /// them with.
    struct Request {
        std::vector<TrackGroup> groups;               ///< groups to build
        const or_segments_vtable *plugin = nullptr;   ///< segments plugin
        std::vector<double> values;                   ///< its parameter values
        const or_input_vtable *input = nullptr;       ///< input plugin
        void *handle = nullptr;                       ///< its handle
    };

    /// The recorded points of outing `id`, from the cache or loaded now.
    /// @return false if they cannot be read
    bool Points(const Request &request, const std::string &id, const std::vector<or_point> *&points);

    /// Plugin cancel callback: the build in progress was superseded.
    static int Superseded(void *self);

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
    std::string error_;                  ///< error of the last finished build
    unsigned building_generation_ = 0;   ///< generation of the build in progress
    std::map<std::string, std::vector<or_point>> points_;  ///< recorded points by outing; worker only
};

} // namespace core
