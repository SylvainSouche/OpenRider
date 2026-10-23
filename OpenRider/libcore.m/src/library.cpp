#include "core/library.h"

#include "core/plugin_thread.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <functional>

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
    hidden_ids_.clear();
    scanning_ = true;
    worker_ = std::thread(&Library::Run, this, inputs, catalog);
}

std::vector<std::string> Library::HiddenIds()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return hidden_ids_;
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

SourceScan ScanSource(InputSet &inputs, const TypeCatalog &catalog, size_t source, const std::vector<or_outing> &earlier,
                      const std::function<void(size_t, size_t)> &progress)
{
    SourceScan scan;
    const InputSet::Source &from = inputs.Sources()[source];
    struct Ctx {
        std::vector<or_outing> outings;
        const std::function<void(size_t, size_t)> *progress;
    } ctx{{}, &progress};
    auto cb = [](void *user, const or_outing *o, size_t index, size_t total) -> int {
        Ctx *c = static_cast<Ctx *>(user);
        c->outings.push_back(*o);
        if (*c->progress)
            (*c->progress)(index + 1, total);
        return 0;
    };
    char err[512] = "";
    int rc = 0;
    if (from.vtable && from.vtable->list_outings)
        CallPlugin(from.vtable, [&] { rc = from.vtable->list_outings(from.handle, cb, &ctx, err, sizeof err); });
    if (rc != 0)
        scan.error = from.name + ": " + (err[0] ? err : "input plugin failed");
    for (const or_outing &o : ctx.outings) {
        if (std::any_of(earlier.begin(), earlier.end(), [&](const or_outing &e) { return SameOuting(e, o); })) {
            scan.hidden_ids.push_back(o.id);
            continue;
        }
        inputs.Assign(o.id, source);
        scan.outings.push_back(
            {o, catalog.Resolve(from.types, o.source_type, o.fit_sport, o.fit_sub_sport), source, !from.confirm_types});
    }
    return scan;
}

SourceScan ScanSource(InputSet &inputs, const TypeCatalog &catalog, size_t source, const std::vector<or_outing> &earlier)
{
    return ScanSource(inputs, catalog, source, earlier, {});
}

void Library::Run(InputSet *inputs, const TypeCatalog *catalog)
{
    std::vector<or_outing> earlier;  // outings of the sources already read
    size_t counted = 0;              // outings announced by those sources
    for (size_t s = 0; s < inputs->Sources().size(); ++s) {
        size_t announced = 0;
        SourceScan scan = ScanSource(*inputs, *catalog, s, earlier, [&](size_t done, size_t total) {
            announced = total;
            total_ = counted + total;
            done_ = counted + done;
        });
        hidden_ += scan.hidden_ids.size();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            hidden_ids_.insert(hidden_ids_.end(), scan.hidden_ids.begin(), scan.hidden_ids.end());
        }
        for (const Outing &outing : scan.outings)
            earlier.push_back(outing.raw);
        counted += announced;
        std::lock_guard<std::mutex> lock(mutex_);
        pending_.insert(pending_.end(), scan.outings.begin(), scan.outings.end());
        if (!scan.error.empty())
            error_ += (error_.empty() ? "" : "; ") + scan.error;
    }
    scanning_ = false;
}

} // namespace core
