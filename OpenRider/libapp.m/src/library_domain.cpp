#include "app/library_domain.h"

#include "app/reports.h"

#include "core/log.h"

#include <cstdio>

namespace app {

void LibraryDomain::Start(const std::string &source)
{
    if (!input_.Ready())
        return;
    library_.StartScan(input_.Vtable(), input_.Handle(), &model_.types, source);
    model_.scanning = true;
    core::LogLine("scanning %s", model_.input_name.c_str());
}

bool LibraryDomain::Handles(IntentKind) const { return false; }

void LibraryDomain::Handle(const Intent &, Intents &) {}

// @impl 0f87-6acd-ce40-0dd9
void LibraryDomain::Tick(Intents &)
{
    const std::string scan_error = library_.Take(model_.outings);
    model_.scanning = library_.Scanning();
    model_.scanned = library_.Scanned();
    model_.scan_total = library_.Total();
    char status[160];
    if (model_.scanning) {
        std::snprintf(status, sizeof status, "Scanning %s... %zu / %zu", model_.input_name.c_str(), model_.scanned,
                      model_.scan_total);
        model_.status = status;
        return;
    }
    if (input_.Ready() && !scan_reported_) {
        scan_reported_ = true;
        if (!scan_error.empty())
            ReportError(model_, "Scanning " + model_.input_name + " failed: " + scan_error);
        std::snprintf(status, sizeof status, "%zu outings from %s", model_.outings.size(), model_.input_name.c_str());
        model_.status = status;
        core::LogLine("%s", status);
        // Then every track, for the overview map.
        std::vector<std::string> ids;
        for (const core::Outing &outing : model_.outings)
            ids.push_back(outing.raw.id);
        overview_.Start(input_.Vtable(), input_.Handle(), ids);
    }
    if (overview_.Take(model_.tracks) > 0)
        ++model_.tracks_version;
    model_.tracks_loading = overview_.Loading();
    model_.tracks_done = overview_.Done();
    model_.tracks_total = overview_.Total();
    if (model_.tracks_loading) {
        std::snprintf(status, sizeof status, "Loading tracks for the overview... %zu / %zu", model_.tracks_done,
                      model_.tracks_total);
        model_.status = status;
    } else if (scan_reported_ && !tracks_reported_ && model_.tracks_total > 0) {
        tracks_reported_ = true;
        std::snprintf(status, sizeof status, "%zu outings, %zu tracks", model_.outings.size(), model_.tracks.size());
        model_.status = status;
        core::LogLine("%s", status);
    }
}

void LibraryDomain::Shutdown()
{
    library_.Wait();
    overview_.Stop();
}

} // namespace app
