#include "app/app_controller.h"

#include "core/log.h"
#include "core/segment_passages.h"
#include "core/outing_selection.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>

namespace app {

namespace {

constexpr double kPassageDistanceM = 30.0;  // a point this close to the segment's line is on it
constexpr double kPassageCoverage = 0.8;    // a passage covers this much of the segment at least

} // namespace

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

void AppController::AddSegmentMethod(const or_segments_vtable *method)
{
    SegmentMethod state;
    state.vtable = method;
    for (size_t i = 0; i < method->param_count; ++i)
        state.values.push_back(method->params[i].default_value);
    model_.segment_methods.push_back(state);
    if (model_.segment_method < 0)
        model_.segment_method = 0;
}

// @impl 0f87-6ace-35a8-5c96
void AppController::OpenComparison(int group, int segment)
{
    if (!input_ || group < 0 || group >= static_cast<int>(model_.segment_groups.size()))
        return;
    const core::SegmentNetwork &network = model_.segment_groups[static_cast<size_t>(group)].network;
    if (segment < 0 || segment >= static_cast<int>(network.segments.size()))
        return;
    SegmentComparison &comparison = model_.comparison;
    comparison = SegmentComparison{true, comparison.generation + 1, model_.segment_groups[static_cast<size_t>(group)].sport,
                                   network.segments[static_cast<size_t>(segment)], {}};
    // Every passage of each outing along the segment, from its recorded
    // points (loaded now: a few dozen outings at most).
    for (const std::string &id : comparison.segment.route_ids) {
        size_t outing = model_.outings.size();
        for (size_t i = 0; i < model_.outings.size(); ++i)
            if (id == model_.outings[i].raw.id)
                outing = i;
        if (outing == model_.outings.size())
            continue;
        std::vector<or_point> points;
        char error[256] = "";
        auto append_point = [](void *user, const or_point *point) -> int {
            static_cast<std::vector<or_point> *>(user)->push_back(*point);
            return 0;
        };
        if (input_->load_points(handle_, id.c_str(), append_point, &points, error, sizeof error) != 0)
            continue;
        int pass = 0;
        for (core::SegmentPassage &passage : core::FindPassages(points, comparison.segment, kPassageDistanceM, kPassageCoverage)) {
            ComparedRun run;
            run.outing = outing;
            run.pass = ++pass;
            run.forward = passage.forward;
            run.points.assign(points.begin() + static_cast<long>(passage.first),
                              points.begin() + static_cast<long>(passage.last) + 1);
            // Time and distance from the entry into the segment.
            const double t0 = run.points.front().t_s, d0 = run.points.front().dist_m;
            for (or_point &point : run.points) {
                point.t_s -= t0;
                if (!std::isnan(d0))
                    point.dist_m -= d0;
            }
            run.duration_s = run.points.back().t_s;
            run.along_m = std::move(passage.along_m);
            comparison.runs.push_back(std::move(run));
        }
    }
    std::stable_sort(comparison.runs.begin(), comparison.runs.end(), [&](const ComparedRun &a, const ComparedRun &b) {
        const int order = std::strcmp(model_.outings[a.outing].raw.start_local, model_.outings[b.outing].raw.start_local);
        return order != 0 ? order > 0 : a.pass < b.pass;
    });
    core::LogLine("comparing segment %d: %zu passages", segment, comparison.runs.size());
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
    networks_.Stop();
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
    UpdateSegments();
}

// @impl 0f87-6acd-d9d9-da69
// The networks follow the overview: the outings passing the map filter
// (type and dates), grouped by sport since only similar activities share
// segments. A change of filter or of the loaded tracks starts a new build;
// the previous networks stay shown until it finishes.
void AppController::UpdateSegments()
{
    if (!model_.show_segments || model_.tracks_loading || model_.tracks.empty() || model_.segment_method < 0) {
        model_.segments_building = networks_.Building();
        return;
    }
    const core::OutingFilter filter = model_.MapFilter();
    const std::string filter_basis = filter.type_key + "|" + filter.from_date + "|" + filter.to_date + "|" +
                              std::to_string(model_.tracks.size());
    const SegmentMethod &method = model_.segment_methods[static_cast<size_t>(model_.segment_method)];
    std::string settings = method.vtable->id;
    for (double value : method.values)
        settings += "|" + std::to_string(value);
    const std::string basis = filter_basis + "|" + settings;
    if (basis != segments_basis_) {
        segments_basis_ = basis;
        std::map<std::string, std::string> sport_of;  // outing id -> sport, for the shown outings
        for (size_t i : core::SelectOutings(model_.outings, filter, core::SortKey::Date, true))
            sport_of[model_.outings[i].raw.id] = model_.outings[i].type.sport;
        std::map<std::string, core::TrackGroup> groups;
        for (const core::OverviewTrack &track : model_.tracks) {
            const auto shown = sport_of.find(track.id);
            if (shown == sport_of.end())
                continue;
            core::TrackGroup &group = groups[shown->second];
            group.sport = shown->second;
            group.tracks.push_back(track);
        }
        std::vector<core::TrackGroup> to_build;
        for (auto &entry : groups)
            if (entry.second.tracks.size() >= 2)  // a segment needs two routes
                to_build.push_back(std::move(entry.second));
        networks_.Start(std::move(to_build), method.vtable, method.values);
        core::LogLine("building segment networks: %s", basis.c_str());
    }
    if (networks_.Take(model_.segment_groups)) {
        const std::string error = networks_.Error();
        if (!error.empty())
            Report("Segment identification failed: " + error);
        size_t shared = 0;
        for (const core::SegmentGroup &group : model_.segment_groups)
            for (const core::NetworkSegment &segment : group.network.segments)
                shared += segment.Shared() ? 1 : 0;
        char status[160];
        std::snprintf(status, sizeof status, "%zu shared segments in %zu activity groups", shared,
                      model_.segment_groups.size());
        model_.status = status;
        core::LogLine("%s", status);
    }
    model_.segments_building = networks_.Building();
    if (model_.segments_building)
        model_.status = "Finding common segments...";
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
        if (intent.i >= 0)
            model_.comparison.open = false;  // the panel shows the outing
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
    case IntentKind::SetShowSegments:
        model_.show_segments = intent.i != 0;
        UpdateSegments();
        break;
    case IntentKind::OpenSegmentComparison:
        OpenComparison(intent.i, intent.j);
        break;
    case IntentKind::CloseSegmentComparison:
        model_.comparison.open = false;
        model_.comparison.runs.clear();
        break;
    case IntentKind::SelectSegmentMethod:
        if (intent.i >= 0 && intent.i < static_cast<int>(model_.segment_methods.size())) {
            model_.segment_method = intent.i;
            UpdateSegments();
        }
        break;
    case IntentKind::SetSegmentParam:
        if (model_.segment_method >= 0) {
            SegmentMethod &method = model_.segment_methods[static_cast<size_t>(model_.segment_method)];
            if (intent.i >= 0 && intent.i < static_cast<int>(method.values.size())) {
                const or_filter_param &param = method.vtable->params[intent.i];
                method.values[static_cast<size_t>(intent.i)] = std::clamp(intent.x, param.min_value, param.max_value);
                UpdateSegments();
            }
        }
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
