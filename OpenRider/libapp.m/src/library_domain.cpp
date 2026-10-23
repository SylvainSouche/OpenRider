#include "app/library_domain.h"

#include "app/reports.h"

#include "core/file_import.h"
#include "core/log.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>

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
           kind == IntentKind::ConfirmOutingTypes || kind == IntentKind::ImportFiles ||
           kind == IntentKind::ImportAccept || kind == IntentKind::ImportCancel;
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
        for (size_t i = 0; i < model_.outings.size(); ++i) {
            core::Outing &outing = model_.outings[i];
            if (!outing.confirmed && static_cast<int>(i) != model_.importing) {  // the import is asked apart
                outing.confirmed = true;
                types_.Set(outing.raw.id, outing.type);
                ++settled;
            }
        }
        if (settled > 0) {
            ++model_.outings_version;
            if (const openrider::sdk::Status saved = types_.Save(); !saved.Ok())
                ReportError(model_, "Outing types: " + saved.error);
            core::LogLine("%zu outing types confirmed", settled);
        }
        break;
    }
    case IntentKind::ImportFiles:
        model_.import_queue.insert(model_.import_queue.end(), intent.ids.begin(), intent.ids.end());
        NextImport();
        break;
    case IntentKind::ImportAccept:
        if (model_.importing >= 0 && model_.importing < static_cast<int>(model_.outings.size())) {
            core::Outing &outing = model_.outings[static_cast<size_t>(model_.importing)];
            outing.confirmed = true;
            types_.Set(outing.raw.id, outing.type);
            if (const openrider::sdk::Status saved = types_.Save(); !saved.Ok())
                ReportError(model_, "Outing types: " + saved.error);
            core::LogLine("imported %s as %s (%s)", model_.importing_file.c_str(), outing.raw.id, outing.type.Key().c_str());
            imported_ = true;
            ++model_.outings_version;
        }
        model_.importing = -1;
        NextImport();
        break;
    case IntentKind::ImportCancel:
        if (model_.importing >= 0 && model_.importing < static_cast<int>(model_.outings.size())) {
            std::remove(import_copy_.c_str());
            model_.outings.erase(model_.outings.begin() + model_.importing);
            if (model_.selected >= model_.importing)
                model_.selected = model_.selected == model_.importing ? -1 : model_.selected - 1;
            ++model_.outings_version;
            core::LogLine("import of %s cancelled", model_.importing_file.c_str());
        }
        model_.importing = -1;
        NextImport();
        break;
    default:
        break;
    }
}

void LibraryDomain::NextImport()
{
    core::InputSet &inputs = input_.Set();
    size_t target = inputs.Sources().size();
    for (size_t s = 0; s < inputs.Sources().size() && target == inputs.Sources().size(); ++s)
        if (!inputs.Sources()[s].import_dir.empty())
            target = s;
    while (model_.importing < 0 && !model_.import_queue.empty()) {
        const std::string file = model_.import_queue.front();
        model_.import_queue.erase(model_.import_queue.begin());
        if (target == inputs.Sources().size()) {
            ReportError(model_, "No source takes imported files");
            model_.import_queue.clear();
            break;
        }
        std::string error;
        const std::string copy = core::CopyIntoFolder(file, inputs.Sources()[target].import_dir, error);
        if (copy.empty()) {
            ReportError(model_, "Import: " + error);
            continue;
        }
        // The source read again: the new outing is the one not known yet;
        // duplicates of the other sources' outings are left out.
        std::set<std::string> known;
        std::vector<or_outing> others;
        for (const core::Outing &outing : model_.outings) {
            known.insert(outing.raw.id);
            if (outing.source != target)
                others.push_back(outing.raw);
        }
        const core::SourceScan scan = core::ScanSource(inputs, model_.types, target, others);
        const core::Outing *fresh = nullptr;
        for (const core::Outing &outing : scan.outings)
            if (!known.count(outing.raw.id))
                fresh = &outing;
        const std::string name = std::filesystem::path(file).filename().string();
        if (!fresh) {
            const bool duplicate = std::any_of(scan.hidden_ids.begin(), scan.hidden_ids.end(),
                                               [&](const std::string &id) { return !hidden_ids_.count(id); });
            std::remove(copy.c_str());
            ReportError(model_, name + (duplicate ? ": this outing is already in the library, not imported"
                                                  : ": no track in this file, not imported"));
            continue;
        }
        core::Outing outing = *fresh;
        if (types_.Find(outing.raw.id, outing.type))
            outing.confirmed = true;
        model_.outings.push_back(outing);
        ++model_.outings_version;
        model_.importing = static_cast<int>(model_.outings.size()) - 1;
        model_.importing_file = file;
        import_copy_ = copy;
    }
    if (model_.importing < 0 && model_.import_queue.empty() && imported_) {
        imported_ = false;
        LoadOverview();
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
        for (const std::string &id : library_.HiddenIds())
            hidden_ids_.insert(id);
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
