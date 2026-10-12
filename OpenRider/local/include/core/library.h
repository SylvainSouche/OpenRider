// The outing library: outings read from an input plugin on a background
// thread, with progress the GUI can poll, plus sorting and filtering.
#pragma once

#include "core/types.h"

#include <openrider/input.h>

#include <atomic>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace core {

struct Outing {
    or_outing raw;        // as delivered by the input plugin
    ActivityType type;    // resolved type
};

enum class SortKey { Date, Name, Type, Distance, Duration, Ascent };

struct OutingFilter {
    std::string type_key;   // ActivityType::key(), "" = all
    std::string from_date;  // "YYYY-MM-DD", "" = open
    std::string to_date;    // "YYYY-MM-DD", inclusive, "" = open
    const std::set<std::string> *only_ids = nullptr;  // e.g. tracks visible on the map
};

bool outing_matches(const Outing &o, const OutingFilter &f);
// Indices of `outings` matching `f`, ordered by `key`.
std::vector<size_t> select_outings(const std::vector<Outing> &outings,
                                   const OutingFilter &f, SortKey key, bool ascending);

class Library {
public:
    ~Library();

    // Starts reading all outings in the background. `source` names the
    // correspondence table used to resolve types (e.g. "garmin_connect").
    void start_scan(const or_input_vtable *input, void *handle,
                    const TypeCatalog *catalog, const std::string &source);

    bool scanning() const { return scanning_.load(); }
    // Progress in [0,1], or -1 while the total is unknown.
    float progress() const;
    size_t scanned() const { return done_.load(); }
    size_t total() const { return total_.load(); }

    // Moves outings read so far into `out` (appending); returns the error of
    // a finished scan, if any.
    std::string take(std::vector<Outing> &out);

private:
    void run(const or_input_vtable *input, void *handle, const TypeCatalog *catalog,
             std::string source);

    std::thread worker_;
    std::atomic<bool> scanning_{false};
    std::atomic<size_t> done_{0};
    std::atomic<size_t> total_{0};
    std::mutex mutex_;
    std::vector<Outing> pending_;
    std::string error_;
};

} // namespace core
