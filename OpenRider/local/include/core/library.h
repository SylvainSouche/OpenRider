/// @file
/// The outing library: outings read from an input plugin on a background
/// thread, with progress the GUI can poll.
#pragma once

#include "core/activity_type.h"
#include "core/type_catalog.h"

#include <openrider/input.h>

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace core {

/// One outing of the library.
struct Outing {
    or_outing raw;      ///< as delivered by the input plugin
    ActivityType type;  ///< resolved type
};

/// Reads every outing of an input plugin in the background. The owner polls
/// Progress() and collects outings with Take() while the scan runs.
class Library {
public:
    /// Waits for a running scan.
    ~Library();

    /// Starts reading all outings in the background.
    /// @param input the input plugin's vtable
    /// @param handle the plugin's open handle; must outlive the scan
    /// @param catalog resolves outing types; must outlive the scan
    /// @param source correspondence table used to resolve types (e.g. "garmin_connect")
    void StartScan(const or_input_vtable *input, void *handle,
                   const TypeCatalog *catalog, const std::string &source);

    /// A scan is running.
    bool Scanning() const { return scanning_.load(); }

    /// Blocks until a running scan has finished.
    void Wait();

    /// Progress in [0,1], or -1 while the total is unknown.
    float Progress() const;

    /// Outings read so far by the running or last scan.
    size_t Scanned() const { return done_.load(); }

    /// Number of outings the input announced; 0 while unknown.
    size_t Total() const { return total_.load(); }

    /// Moves outings read so far into `out` (appending).
    /// @return the error of a finished scan, "" if none
    std::string Take(std::vector<Outing> &out);

private:
    /// The scan, run on the worker thread.
    void Run(const or_input_vtable *input, void *handle, const TypeCatalog *catalog,
             std::string source);

    std::thread worker_;                 ///< runs Run()
    std::atomic<bool> scanning_{false};  ///< a scan is running
    std::atomic<size_t> done_{0};        ///< outings read
    std::atomic<size_t> total_{0};       ///< outings announced
    std::mutex mutex_;              ///< guards pending_ and error_
    std::vector<Outing> pending_;   ///< read, not yet taken
    std::string error_;             ///< error of the finished scan
};

} // namespace core
