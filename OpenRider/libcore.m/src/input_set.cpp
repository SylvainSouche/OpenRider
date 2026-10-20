#include "core/input_set.h"

#include "core/plugin_thread.h"

namespace core {

size_t InputSet::Add(Source source)
{
    sources_.push_back(std::move(source));
    return sources_.size() - 1;
}

void InputSet::Assign(const std::string &id, size_t source)
{
    std::lock_guard<std::mutex> lock(mutex_);
    owner_[id] = source;
}

size_t InputSet::SourceOf(const std::string &id) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = owner_.find(id);
    return found == owner_.end() ? 0 : found->second;
}

openrider::sdk::Status InputSet::LoadPoints(const std::string &id, std::vector<or_point> &points) const
{
    points.clear();
    if (sources_.empty())
        return openrider::sdk::Status::Failure("no input source");
    const Source &source = sources_[SourceOf(id)];
    if (!source.vtable || !source.vtable->load_points)
        return openrider::sdk::Status::Failure("input " + source.name + " cannot read points");
    char error[512] = "";
    auto append = [](void *user, const or_point *point) -> int {
        static_cast<std::vector<or_point> *>(user)->push_back(*point);
        return 0;
    };
    int rc = 0;
    CallPlugin(source.vtable, [&] { rc = source.vtable->load_points(source.handle, id.c_str(), append, &points, error, sizeof error); });
    if (rc != 0)
        return openrider::sdk::Status::Failure(error[0] ? error : "cannot read points of " + id);
    return openrider::sdk::Status::Success();
}

void InputSet::CloseAll()
{
    for (Source &source : sources_) {
        if (source.vtable && source.vtable->close && source.handle)
            CallPlugin(source.vtable, [&] { source.vtable->close(source.handle); });
        source.handle = nullptr;
    }
}

} // namespace core
