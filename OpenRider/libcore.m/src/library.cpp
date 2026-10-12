#include "core/library.h"

#include <algorithm>
#include <cstring>

namespace core {

bool outing_matches(const Outing &o, const OutingFilter &f)
{
    if (!f.type_key.empty() && o.type.key() != f.type_key)
        return false;
    if (f.only_ids && f.only_ids->count(o.raw.id) == 0)
        return false;
    const std::string day(o.raw.start_local, std::min<size_t>(10, std::strlen(o.raw.start_local)));
    if (!f.from_date.empty() && day < f.from_date)
        return false;
    if (!f.to_date.empty() && day > f.to_date)
        return false;
    return true;
}

// @impl 0f87-6acd-24ce-ac37
std::vector<size_t> select_outings(const std::vector<Outing> &outings, const OutingFilter &f,
                                   SortKey key, bool ascending)
{
    std::vector<size_t> idx;
    for (size_t i = 0; i < outings.size(); ++i)
        if (outing_matches(outings[i], f))
            idx.push_back(i);
    auto less = [&](size_t a, size_t b) {
        const or_outing &x = outings[a].raw;
        const or_outing &y = outings[b].raw;
        switch (key) {
        case SortKey::Name: return std::strcmp(x.name, y.name) < 0;
        case SortKey::Type: return outings[a].type.key() < outings[b].type.key();
        case SortKey::Distance: return x.distance_m < y.distance_m;
        case SortKey::Duration: return x.duration_s < y.duration_s;
        case SortKey::Ascent: return x.ascent_m < y.ascent_m;
        case SortKey::Date: break;
        }
        return std::strcmp(x.start_local, y.start_local) < 0;
    };
    std::stable_sort(idx.begin(), idx.end(), [&](size_t a, size_t b) {
        return ascending ? less(a, b) : less(b, a);
    });
    return idx;
}

Library::~Library()
{
    if (worker_.joinable())
        worker_.join();
}

void Library::start_scan(const or_input_vtable *input, void *handle, const TypeCatalog *catalog,
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
    worker_ = std::thread(&Library::run, this, input, handle, catalog, source);
}

float Library::progress() const
{
    const size_t t = total_.load();
    return t == 0 ? -1.0f : static_cast<float>(done_.load()) / static_cast<float>(t);
}

std::string Library::take(std::vector<Outing> &out)
{
    std::lock_guard<std::mutex> lock(mutex_);
    out.insert(out.end(), pending_.begin(), pending_.end());
    pending_.clear();
    return scanning_.load() ? std::string() : error_;
}

void Library::run(const or_input_vtable *input, void *handle, const TypeCatalog *catalog,
                  std::string source)
{
    struct Ctx {
        Library *self;
        const TypeCatalog *catalog;
        const std::string *source;
    } ctx{this, catalog, &source};

    auto cb = [](void *user, const or_outing *o, size_t index, size_t total) -> int {
        Ctx *c = static_cast<Ctx *>(user);
        Outing out{*o, c->catalog->resolve(*c->source, o->source_type, o->fit_sport,
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
    const int rc = input->list_outings(handle, cb, &ctx, err, sizeof err);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (rc != 0)
            error_ = err[0] ? err : "input plugin failed";
    }
    scanning_ = false;
}

} // namespace core
