#include "core/network_builder.h"

namespace core {

NetworkBuilder::~NetworkBuilder() { Stop(); }

void NetworkBuilder::Stop()
{
    stop_ = true;
    if (worker_.joinable())
        worker_.join();
    stop_ = false;
    building_ = false;
}

void NetworkBuilder::Start(std::vector<TrackGroup> groups, const SegmentParams &params)
{
    Stop();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        result_ready_ = false;
    }
    building_ = true;
    worker_ = std::thread(&NetworkBuilder::Run, this, std::move(groups), params);
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

void NetworkBuilder::Run(std::vector<TrackGroup> groups, SegmentParams params)
{
    std::vector<SegmentGroup> built;
    for (TrackGroup &group : groups) {
        if (stop_.load())
            return;  // superseded: nothing to publish
        built.push_back(SegmentGroup{group.sport, BuildNetwork(group.tracks, params)});
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        result_ = std::move(built);
        result_ready_ = true;
    }
    building_ = false;
}

} // namespace core
