#include "core/library.h"

#include "core/plugin_thread.h"

#include <algorithm>

namespace core {

Library::~Library() { Wait(); }

void Library::Wait()
{
    if (worker_.joinable())
        worker_.join();
}

void Library::StartScan(const or_input_vtable *input, void *handle, const TypeCatalog *catalog,
                         const std::string &source)
{
    if (scanning_.load())
        return;
    if (worker_.joinable())
        worker_.join();
    done_ = 0;
    total_ = 0;
    error_.clear();
    scanning_ = true;
    worker_ = std::thread(&Library::Run, this, input, handle, catalog, source);
}

float Library::Progress() const
{
    const size_t t = total_.load();
    return t == 0 ? -1.0f : static_cast<float>(done_.load()) / static_cast<float>(t);
}

std::string Library::Take(std::vector<Outing> &out)
{
    std::lock_guard<std::mutex> lock(mutex_);
    out.insert(out.end(), pending_.begin(), pending_.end());
    pending_.clear();
    return scanning_.load() ? std::string() : error_;
}

void Library::Run(const or_input_vtable *input, void *handle, const TypeCatalog *catalog,
                  std::string source)
{
    struct Ctx {
        Library *self;
        const TypeCatalog *catalog;
        const std::string *source;
    } ctx{this, catalog, &source};

    auto cb = [](void *user, const or_outing *o, size_t index, size_t total) -> int {
        Ctx *c = static_cast<Ctx *>(user);
        Outing out{*o, c->catalog->Resolve(*c->source, o->source_type, o->fit_sport,
                                           o->fit_sub_sport)};
        {
            std::lock_guard<std::mutex> lock(c->self->mutex_);
            c->self->pending_.push_back(out);
        }
        c->self->total_ = total;
        c->self->done_ = index + 1;
        return 0;
    };
    char err[512] = "";
    int rc = 0;
    CallPlugin(input, [&] { rc = input->list_outings(handle, cb, &ctx, err, sizeof err); });
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (rc != 0)
            error_ = err[0] ? err : "input plugin failed";
    }
    scanning_ = false;
}

} // namespace core
