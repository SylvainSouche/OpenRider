#include "app/app_controller.h"

#include "core/log.h"
#include "core/outing_selection.h"

#include <cstdio>

namespace app {

AppController::~AppController() { Shutdown(); }

void AppController::LoadTypes(const std::string &labels_json, const std::string &source,
                               const std::string &source_json)
{
    if (labels_json.empty() || source_json.empty()) {
        Report("Cannot read the activity type tables");
        return;
    }
    core::Status status = model_.types.LoadLabels(labels_json);
    if (!status.Ok())
        Report(status.error);
    status = model_.types.LoadSource(source, source_json);
    if (!status.Ok())
        Report(status.error);
}

void AppController::SetInput(const or_input_vtable *input, void *handle, std::string name)
{
    input_ = input;
    handle_ = handle;
    model_.input_name = std::move(name);
}

void AppController::AddFilter(const or_filter_vtable *filter)
{
    FilterState state;
    state.vtable = filter;
    for (size_t i = 0; i < filter->param_count; ++i)
        state.values.push_back(filter->params[i].default_value);
    model_.filters.push_back(state);
}

void AppController::Start()
{
    if (!input_)
        return;
    library_.StartScan(input_, handle_, &model_.types, "garmin_connect");
    model_.scanning = true;
    core::LogLine("scanning %s", model_.input_name.c_str());
}

void AppController::Shutdown()
{
    library_.Wait();
    overview_.Stop();
}

void AppController::Report(const std::string &error)
{
    core::LogLine("error: %s", error.c_str());
    model_.errors.push_back(error);
}

// @impl 0f87-6acd-ce40-0dd9
void AppController::Tick(Intents &out)
{
    (void)out;
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
    if (input_ && !scan_reported_) {
        scan_reported_ = true;
        if (!scan_error.empty())
            Report("Scanning " + model_.input_name + " failed: " + scan_error);
        std::snprintf(status, sizeof status, "%zu outings from %s", model_.outings.size(), model_.input_name.c_str());
        model_.status = status;
        core::LogLine("%s", status);
        // Then every track, for the overview map.
        std::vector<std::string> ids;
        for (const core::Outing &outing : model_.outings)
            ids.push_back(outing.raw.id);
        overview_.Start(input_, handle_, ids);
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

void AppController::Select(int index)
{
    if (index >= static_cast<int>(model_.outings.size()))
        index = -1;
    model_.selected = index;
    const core::Outing *outing = model_.SelectedOuting();
    if (!outing || !input_ || model_.points_id == outing->raw.id)
        return;
    model_.raw_points.clear();
    model_.points_id = outing->raw.id;
    char error[512] = "";
    auto append_point = [](void *user, const or_point *point) -> int {
        static_cast<std::vector<or_point> *>(user)->push_back(*point);
        return 0;
    };
    if (input_->load_points(handle_, outing->raw.id, append_point, &model_.raw_points, error, sizeof error) != 0)
        Report(std::string("Cannot load points of ") + outing->raw.name + ": " + error);
    core::LogLine("loaded %zu points for %s", model_.raw_points.size(), outing->raw.id);
    ApplyFilters();
}

// Re-applies the enabled track filters to the raw points (non-destructive)
// and recomputes the statistics (moving time on the raw points).
void AppController::ApplyFilters()
{
    model_.points = model_.raw_points;
    // Each filter marks the points to keep; the next filter sees only those.
    for (const FilterState &filter : model_.filters) {
        if (!filter.enabled || filter.vtable->kind != OR_FILTER_TRACK || model_.points.empty())
            continue;
        std::vector<unsigned char> keep(model_.points.size(), 1);
        char error[256] = "";
        if (filter.vtable->apply_track(model_.points.data(), model_.points.size(), filter.values.data(),
                                       keep.data(), error, sizeof error) != 0) {
            Report(std::string("Filter ") + filter.vtable->label + " failed: " + error);
            continue;
        }
        std::vector<or_point> kept;
        for (size_t i = 0; i < model_.points.size(); ++i)
            if (keep[i])
                kept.push_back(model_.points[i]);
        core::LogLine("filter %s kept %zu of %zu points", filter.vtable->id, kept.size(), model_.points.size());
        model_.points.swap(kept);
    }
    model_.computed = core::ComputeStats(model_.points, core::MotionParams{}, &model_.raw_points);
}

void AppController::Handle(const Intent &intent, Intents &out)
{
    (void)out;
    switch (intent.kind) {
    case IntentKind::SelectOuting:
        Select(intent.i);
        break;
    case IntentKind::SetTypeFilter:
        model_.query.type_key = intent.text;
        break;
    case IntentKind::SetDateFrom:
        model_.query.from_date = intent.text;
        break;
    case IntentKind::SetDateTo:
        model_.query.to_date = intent.text;
        break;
    case IntentKind::SetOnlyVisible:
        model_.query.only_visible = intent.i != 0;
        if (model_.query.only_visible)
            model_.selected = -1;  // the extent is the overview map's
        break;
    case IntentKind::SetVisibleIds:
        model_.query.visible_ids = std::set<std::string>(intent.ids.begin(), intent.ids.end());
        break;
    case IntentKind::ToggleFilter:
        if (intent.i >= 0 && intent.i < static_cast<int>(model_.filters.size())) {
            model_.filters[static_cast<size_t>(intent.i)].enabled = intent.j != 0;
            ApplyFilters();
        }
        break;
    case IntentKind::SetFilterParam:
        if (intent.i >= 0 && intent.i < static_cast<int>(model_.filters.size())) {
            FilterState &filter = model_.filters[static_cast<size_t>(intent.i)];
            if (intent.j >= 0 && intent.j < static_cast<int>(filter.values.size())) {
                filter.values[static_cast<size_t>(intent.j)] = intent.x;
                if (filter.enabled)
                    ApplyFilters();
            }
        }
        break;
    case IntentKind::ResetFilterParams:
        if (intent.i >= 0 && intent.i < static_cast<int>(model_.filters.size())) {
            FilterState &filter = model_.filters[static_cast<size_t>(intent.i)];
            for (size_t k = 0; k < filter.values.size(); ++k)
                filter.values[k] = filter.vtable->params[k].default_value;
            if (filter.enabled)
                ApplyFilters();
        }
        break;
    case IntentKind::DismissError:
        if (!model_.errors.empty())
            model_.errors.erase(model_.errors.begin());
        break;
    case IntentKind::ReportError:
        Report(intent.text);
        break;
    case IntentKind::Quit:
        model_.quit = true;
        break;
    default:
        break;  // not the model's
    }
}

} // namespace app
