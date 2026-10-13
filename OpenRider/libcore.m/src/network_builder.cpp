#include "core/network_builder.h"

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
void NetworkBuilder::Start(std::vector<TrackGroup> groups, const SegmentParams &params)
{
    std::lock_guard<std::mutex> lock(mutex_);
    request_ = Request{std::move(groups), params};
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
        std::vector<SegmentGroup> built;
        bool superseded = false;
        for (TrackGroup &group : request.groups) {
            if (stop_.load() || generation_.load() != generation) {
                superseded = true;
                break;
            }
            built.push_back(SegmentGroup{group.sport, BuildNetwork(group.tracks, request.params)});
        }
        std::lock_guard<std::mutex> lock(mutex_);
        if (!superseded && generation_.load() == generation) {
            result_ = std::move(built);
            result_ready_ = true;
        }
    }
}

} // namespace core
