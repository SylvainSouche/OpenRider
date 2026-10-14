#include "core/overview_loader.h"

#include "core/plugin_thread.h"

namespace core {

OverviewLoader::~OverviewLoader() { Stop(); }

void OverviewLoader::Stop()
{
    stop_ = true;
    if (worker_.joinable())
        worker_.join();
    loading_ = false;
}

void OverviewLoader::Start(const or_input_vtable *input, void *handle, std::vector<std::string> ids)
{
    if (loading_.load())
        return;
    if (worker_.joinable())
        worker_.join();
    stop_ = false;  // a previous Stop() must not end this load at once
    done_ = 0;
    total_ = ids.size();
    loading_ = true;
    worker_ = std::thread(&OverviewLoader::Run, this, input, handle, std::move(ids));
}

size_t OverviewLoader::Take(std::vector<openrider::sdk::OverviewTrack> &out)
{
    std::lock_guard<std::mutex> lock(mutex_);
    const size_t taken = pending_.size();
    for (openrider::sdk::OverviewTrack &t : pending_)
        out.push_back(std::move(t));
    pending_.clear();
    return taken;
}

void OverviewLoader::Run(const or_input_vtable *input, void *handle, std::vector<std::string> ids)
{
    for (const std::string &id : ids) {
        if (stop_.load())
            break;
        std::vector<or_point> pts;
        char err[256] = "";
        auto cb = [](void *user, const or_point *p) -> int {
            static_cast<std::vector<or_point> *>(user)->push_back(*p);
            return 0;
        };
        int rc = 0;
        CallPlugin(input, [&] { rc = input->load_points(handle, id.c_str(), cb, &pts, err, sizeof err); });
        if (rc == 0) {
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
