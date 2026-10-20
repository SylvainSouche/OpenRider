#include "app/library_domain.h"

#include "app/reports.h"

#include "core/log.h"

#include <cstdio>
#include <cstring>

namespace app {

void LibraryDomain::Start()
{
    if (!input_.Ready())
        return;
    library_.StartScan(&input_.Set(), &model_.types);
    model_.scanning = true;
    core::LogLine("scanning %s", model_.input_name.c_str());
}

void LibraryDomain::SetTypesFile(const std::string &path)
{
    const openrider::sdk::Status loaded = types_.Load(path);
    if (!loaded.Ok())
        ReportError(model_, "Outing types: " + loaded.error);
}

bool LibraryDomain::Handles(IntentKind kind) const
{
    return kind == IntentKind::ReloadOverview || kind == IntentKind::SetOutingType ||
           kind == IntentKind::ConfirmOutingTypes;
}

void LibraryDomain::Handle(const Intent &intent, Intents &)
{
    switch (intent.kind) {
    case IntentKind::ReloadOverview:
        if (scan_reported_)
            LoadOverview();
        break;
    case IntentKind::SetOutingType:
        if (intent.i >= 0 && intent.i < static_cast<int>(model_.outings.size()) && !intent.text.empty()) {
            core::Outing &outing = model_.outings[static_cast<size_t>(intent.i)];
            outing.type = core::TypeOfKey(intent.text);
            ++model_.outings_version;  // groups by sport change
            if (outing.confirmed) {    // a settled outing changed: remembered at once
                types_.Set(outing.raw.id, outing.type);
                if (const openrider::sdk::Status saved = types_.Save(); !saved.Ok())
                    ReportError(model_, "Outing types: " + saved.error);
            }
        }
        break;
    case IntentKind::ConfirmOutingTypes: {
        size_t settled = 0;
        for (core::Outing &outing : model_.outings)
            if (!outing.confirmed) {
                outing.confirmed = true;
                types_.Set(outing.raw.id, outing.type);
                ++settled;
            }
        if (settled > 0) {
            ++model_.outings_version;
            if (const openrider::sdk::Status saved = types_.Save(); !saved.Ok())
                ReportError(model_, "Outing types: " + saved.error);
            core::LogLine("%zu outing types confirmed", settled);
        }
        break;
    }
    default:
        break;
    }
}

void LibraryDomain::LoadOverview()
{
    model_.tracks.clear();
    tracks_reported_ = false;
    std::vector<std::string> ids;
    for (const core::Outing &outing : model_.outings)
        ids.push_back(outing.raw.id);
    overview_.Start(&input_.Set(), ids, model_.EnabledFilters());
}

// @impl 0f87-6acd-ce40-0dd9
void LibraryDomain::Tick(Intents &)
{
    const size_t known = model_.outings.size();
    const std::string scan_error = library_.Take(model_.outings);
    if (model_.outings.size() != known) {
        // The types the user settled before override the sources'.
        for (size_t i = known; i < model_.outings.size(); ++i) {
            core::Outing &outing = model_.outings[i];
            if (types_.Find(outing.raw.id, outing.type))
                outing.confirmed = true;
        }
        ++model_.outings_version;
    }
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
        if (library_.Hidden() > 0)
            std::snprintf(status + std::strlen(status), sizeof status - std::strlen(status),
                          " (%zu duplicates hidden)", library_.Hidden());
        model_.status = status;
        core::LogLine("%s", status);
        // Then every track, for the overview map.
        LoadOverview();
    }
    overview_.Take(model_.tracks);
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
