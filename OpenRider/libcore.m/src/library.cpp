#include "core/library.h"

#include "core/plugin_thread.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>

namespace core {

Library::~Library() { Wait(); }

void Library::Wait()
{
    if (worker_.joinable())
        worker_.join();
}

void Library::StartScan(InputSet *inputs, const TypeCatalog *catalog)
{
    if (scanning_.load())
        return;
    if (worker_.joinable())
        worker_.join();
    done_ = 0;
    total_ = 0;
    hidden_ = 0;
    error_.clear();
    scanning_ = true;
    worker_ = std::thread(&Library::Run, this, inputs, catalog);
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

namespace {

// Seconds of a "YYYY-MM-DD HH:MM:SS" time (any fixed zone); NaN if unreadable.
double Seconds(const char *start)
{
    std::tm tm{};
    if (!start || std::sscanf(start, "%d-%d-%d %d:%d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min,
                              &tm.tm_sec) != 6)
        return std::nan("");
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;
    return static_cast<double>(timegm(&tm));
}

// `a` and `b` are the same outing recorded twice: started within a minute,
// distances within 15% when both are known.
bool SameOuting(const or_outing &a, const or_outing &b)
{
    const double ta = Seconds(a.start_local), tb = Seconds(b.start_local);
    if (std::isnan(ta) || std::isnan(tb) || std::fabs(ta - tb) > 60.0)
        return false;
    if (std::isnan(a.distance_m) || std::isnan(b.distance_m) || a.distance_m <= 0.0 || b.distance_m <= 0.0)
        return true;
    return std::fabs(a.distance_m - b.distance_m) <= 0.15 * std::max(a.distance_m, b.distance_m);
}

} // namespace

void Library::Run(InputSet *inputs, const TypeCatalog *catalog)
{
    std::vector<or_outing> earlier;  // outings of the sources already read
    size_t counted = 0;              // outings announced by those sources
    for (size_t s = 0; s < inputs->Sources().size(); ++s) {
        const InputSet::Source &source = inputs->Sources()[s];
        struct Ctx {
            Library *self;
            std::vector<or_outing> outings;
            size_t counted;
        } ctx{this, {}, counted};
        auto cb = [](void *user, const or_outing *o, size_t index, size_t total) -> int {
            Ctx *c = static_cast<Ctx *>(user);
            c->outings.push_back(*o);
            c->self->total_ = c->counted + total;
            c->self->done_ = c->counted + index + 1;
            return 0;
        };
        char err[512] = "";
        int rc = 0;
        if (source.vtable && source.vtable->list_outings)
            CallPlugin(source.vtable, [&] { rc = source.vtable->list_outings(source.handle, cb, &ctx, err, sizeof err); });
        std::vector<Outing> kept;
        for (const or_outing &o : ctx.outings) {
            if (std::any_of(earlier.begin(), earlier.end(), [&](const or_outing &e) { return SameOuting(e, o); })) {
                ++hidden_;
                continue;
            }
            Outing out{o, catalog->Resolve(source.types, o.source_type, o.fit_sport, o.fit_sub_sport), s,
                       !source.confirm_types};
            inputs->Assign(o.id, s);
            kept.push_back(out);
        }
        earlier.insert(earlier.end(), ctx.outings.begin(), ctx.outings.end());
        counted += ctx.outings.size();
        std::lock_guard<std::mutex> lock(mutex_);
        pending_.insert(pending_.end(), kept.begin(), kept.end());
        if (rc != 0)
            error_ += (error_.empty() ? "" : "; ") + source.name + ": " + (err[0] ? err : "input plugin failed");
    }
    scanning_ = false;
}

} // namespace core
