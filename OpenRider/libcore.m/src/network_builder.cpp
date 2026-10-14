#include "core/network_builder.h"

#include "core/plugin_thread.h"

#include <openrider/sdk/segment_exchange.h>

#include <algorithm>
#include <cmath>
#include <set>

namespace core {

NetworkBuilder::~NetworkBuilder() { Stop(); }

void NetworkBuilder::Stop()
{
    stop_ = true;
    if (worker_.joinable())
        worker_.join();
    std::lock_guard<std::mutex> lock(mutex_);
    stop_ = false;
    running_ = false;
    pending_ = false;
    building_ = false;
}

// @impl 0f87-6acd-dc0b-a958
void NetworkBuilder::Start(std::vector<TrackGroup> groups, const or_segments_vtable *plugin,
                           std::vector<double> values, const or_input_vtable *input, void *handle)
{
    std::lock_guard<std::mutex> lock(mutex_);
    request_ = Request{std::move(groups), plugin, std::move(values), input, handle};
    pending_ = true;
    result_ready_ = false;  // an older result is superseded
    ++generation_;          // the running build drops out at its next group
    building_ = true;
    if (running_)
        return;  // the worker picks the request up when its build ends
    // The previous worker has left its loop (running_ is false): joining
    // only waits for its return.
    if (worker_.joinable())
        worker_.join();
    running_ = true;
    worker_ = std::thread(&NetworkBuilder::Run, this);
}

std::string NetworkBuilder::Error()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return error_;
}

int NetworkBuilder::Superseded(void *self)
{
    const NetworkBuilder *builder = static_cast<const NetworkBuilder *>(self);
    return builder->stop_.load() || builder->generation_.load() != builder->building_generation_ ? 1 : 0;
}

bool NetworkBuilder::Take(std::vector<SegmentGroup> &out)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!result_ready_)
        return false;
    out = std::move(result_);
    result_.clear();
    result_ready_ = false;
    return true;
}

bool NetworkBuilder::Points(const Request &request, const std::string &id, const std::vector<or_point> *&points)
{
    auto found = points_.find(id);
    if (found == points_.end()) {
        std::vector<or_point> loaded;
        char error[256] = "";
        auto append = [](void *user, const or_point *point) -> int {
            static_cast<std::vector<or_point> *>(user)->push_back(*point);
            return 0;
        };
        int rc = 1;
        if (request.input && request.input->load_points)
            CallPlugin(request.input, [&] {
                rc = request.input->load_points(request.handle, id.c_str(), append, &loaded, error, sizeof error);
            });
        if (rc != 0)
            return false;
        found = points_.emplace(id, std::move(loaded)).first;
    }
    points = &found->second;
    return true;
}

void NetworkBuilder::Run()
{
    for (;;) {
        Request request;
        unsigned generation;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!pending_ || stop_.load()) {
                running_ = false;
                building_ = false;
                return;
            }
            request = std::move(request_);
            pending_ = false;
            generation = generation_.load();
        }
        building_generation_ = generation;
        // Only the points of the outings asked for now stay cached.
        std::set<std::string> wanted;
        for (const TrackGroup &group : request.groups)
            wanted.insert(group.ids.begin(), group.ids.end());
        for (auto entry = points_.begin(); entry != points_.end();)
            entry = wanted.count(entry->first) ? std::next(entry) : points_.erase(entry);
        std::vector<SegmentGroup> built;
        std::string error;
        bool superseded = false;
        for (TrackGroup &group : request.groups) {
            if (stop_.load() || generation_.load() != generation) {
                superseded = true;
                break;
            }
            // The group's recorded tracks: outings with at least one position.
            std::vector<openrider::sdk::RecordedTrack> tracks;
            for (const std::string &id : group.ids) {
                if (stop_.load() || generation_.load() != generation)
                    break;
                const std::vector<or_point> *points = nullptr;
                if (!Points(request, id, points)) {
                    error = "cannot read the points of outing " + id;
                    continue;
                }
                if (std::any_of(points->begin(), points->end(),
                                [](const or_point &p) { return !std::isnan(p.lat) && !std::isnan(p.lon); }))
                    tracks.push_back({id, *points});
            }
            if (tracks.size() < 2)
                continue;  // a segment needs two routes
            openrider::sdk::Result<openrider::sdk::SegmentNetwork> network;
            CallPlugin(request.plugin, [&] {
                network = openrider::sdk::RunSegments(request.plugin, tracks, request.values, Superseded, this);
            });
            if (stop_.load() || generation_.load() != generation) {
                superseded = true;  // the plugin may have stopped at Superseded()'s request
                break;
            }
            if (!network.Ok()) {
                error = network.error;
                continue;
            }
            built.push_back(SegmentGroup{group.sport, std::move(network.value)});
        }
        std::lock_guard<std::mutex> lock(mutex_);
        if (!superseded && generation_.load() == generation) {
            result_ = std::move(built);
            result_ready_ = true;
            error_ = error;
        }
    }
}

} // namespace core
