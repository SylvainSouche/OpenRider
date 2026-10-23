#include "core/overview_loader.h"


namespace core {

OverviewLoader::~OverviewLoader() { Stop(); }

void OverviewLoader::Stop()
{
    stop_ = true;
    if (worker_.joinable())
        worker_.join();
    loading_ = false;
}

void OverviewLoader::Start(const InputSet *inputs, std::vector<std::string> ids, std::vector<TrackFilter> filters)
{
    stop_ = true;  // a load still running ends at its next outing
    if (worker_.joinable())
        worker_.join();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_.clear();  // tracks of the replaced load
    }
    stop_ = false;  // a previous Stop() must not end this load at once
    done_ = 0;
    total_ = ids.size();
    loading_ = true;
    worker_ = std::thread(&OverviewLoader::Run, this, inputs, std::move(ids), std::move(filters));
}

void OverviewLoader::Take(std::vector<openrider::sdk::OverviewTrack> &out)
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (openrider::sdk::OverviewTrack &t : pending_)
        out.push_back(std::move(t));
    pending_.clear();
}

void OverviewLoader::Run(const InputSet *inputs, std::vector<std::string> ids, std::vector<TrackFilter> filters)
{
    for (const std::string &id : ids) {
        if (stop_.load())
            break;
        std::vector<or_point> pts;
        const bool read = inputs->LoadPoints(id, pts).Ok();
        if (read) {
            FilterPoints(pts, filters);  // a failing filter leaves the points as they are
            openrider::sdk::OverviewTrack t = openrider::sdk::ThinTrack(id, pts, openrider::sdk::kOverviewSpacingM);
            if (!t.lat.empty()) {
                std::lock_guard<std::mutex> lock(mutex_);
                pending_.push_back(std::move(t));
            }
        }
        ++done_;
    }
    loading_ = false;
}

} // namespace core
