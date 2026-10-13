#include "core/network_builder.h"

#include "core/plugin_thread.h"

#include <openrider/sdk/segment_exchange.h>

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
                           std::vector<double> values)
{
    std::lock_guard<std::mutex> lock(mutex_);
    request_ = Request{std::move(groups), plugin, std::move(values)};
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
        std::vector<SegmentGroup> built;
        std::string error;
        bool superseded = false;
        for (TrackGroup &group : request.groups) {
            if (stop_.load() || generation_.load() != generation) {
                superseded = true;
                break;
            }
            openrider::sdk::Result<openrider::sdk::SegmentNetwork> network;
            CallPlugin(request.plugin, [&] {
                network = openrider::sdk::RunSegments(request.plugin, group.tracks, request.values, Superseded, this);
            });
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
