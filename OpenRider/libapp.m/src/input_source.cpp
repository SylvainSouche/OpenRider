#include "app/input_source.h"

#include "core/plugin_thread.h"

namespace app {

void InputSource::Set(const or_input_vtable *vtable, void *handle)
{
    vtable_ = vtable;
    handle_ = handle;
}

openrider::sdk::Status InputSource::LoadPoints(const std::string &id, std::vector<or_point> &points) const
{
    points.clear();
    if (!vtable_)
        return openrider::sdk::Status::Failure("no input plugin");
    char error[512] = "";
    auto append_point = [](void *user, const or_point *point) -> int {
        static_cast<std::vector<or_point> *>(user)->push_back(*point);
        return 0;
    };
    int rc = 0;
    core::CallPlugin(vtable_, [&] { rc = vtable_->load_points(handle_, id.c_str(), append_point, &points, error, sizeof error); });
    if (rc != 0)
        return openrider::sdk::Status::Failure(error);
    return openrider::sdk::Status::Success();
}

} // namespace app
