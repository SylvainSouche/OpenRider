/// @file
/// The outing library: outings read from an input plugin on a background
/// thread, with progress the GUI can poll.
#pragma once

#include "core/activity_type.h"
#include "core/input_set.h"
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
    or_outing raw;          ///< as delivered by the input plugin
    ActivityType type;      ///< resolved type
    size_t source = 0;      ///< index of its source in the InputSet
    bool confirmed = true;  ///< its type is settled (else the user is asked to confirm it)
};

/// The outings of one source, as a scan reads them.
struct SourceScan {
    std::vector<Outing> outings;          ///< read, types resolved, assigned to the source
    std::vector<std::string> hidden_ids;  ///< left out as duplicates of `earlier`
    std::string error;                    ///< the plugin's error, "" if none
};

/// Reads every outing of source `source` of `inputs` now: types resolved
/// with the source's table (unconfirmed if the source asks), each outing
/// assigned to the source; an outing duplicating one of `earlier` (start
/// within a minute, distance within 15% when both are known) left out.
SourceScan ScanSource(InputSet &inputs, const TypeCatalog &catalog, size_t source, const std::vector<or_outing> &earlier);

/// Reads every outing of an input plugin in the background. The owner polls
/// Progress() and collects outings with Take() while the scan runs.
class Library {
public:
    /// Waits for a running scan.
    ~Library();

    /// Starts reading the outings of every source of `inputs` in the
    /// background, one source after the other. Each outing's type resolves
    /// with its source's table; outings of a source whose types are to be
    /// confirmed come unconfirmed. An outing duplicating one of an earlier
    /// source (start within a minute, distance within 15% when both are
    /// known) is left out: the earlier source wins.
    /// @param inputs the sources; must outlive the scan (outing ids are
    ///        assigned to their source in it)
    /// @param catalog resolves outing types; must outlive the scan
    void StartScan(InputSet *inputs, const TypeCatalog *catalog);

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

    /// Outings left out of the finished scan as duplicates.
    size_t Hidden() const { return hidden_.load(); }

    /// Ids of the outings left out of the finished scan as duplicates.
    std::vector<std::string> HiddenIds();

private:
    /// The scan, run on the worker thread.
    void Run(InputSet *inputs, const TypeCatalog *catalog);

    std::thread worker_;                 ///< runs Run()
    std::atomic<bool> scanning_{false};  ///< a scan is running
    std::atomic<size_t> done_{0};        ///< outings read
    std::atomic<size_t> total_{0};       ///< outings announced
    std::atomic<size_t> hidden_{0};      ///< duplicates left out
    std::mutex mutex_;              ///< guards pending_ and error_
    std::vector<Outing> pending_;   ///< read, not yet taken
    std::string error_;             ///< error of the finished scan
    std::vector<std::string> hidden_ids_;  ///< duplicates left out
};

} // namespace core
