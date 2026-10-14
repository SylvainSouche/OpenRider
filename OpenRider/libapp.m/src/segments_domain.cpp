#include "app/segments_domain.h"

#include "app/reports.h"

#include "core/log.h"
#include "core/outing_selection.h"

#include <algorithm>
#include <cstdio>
#include <map>

namespace app {

void SegmentsDomain::AddMethod(const or_segments_vtable *method)
{
    SegmentMethod state;
    state.vtable = method;
    for (size_t i = 0; i < method->param_count; ++i)
        state.values.push_back(method->params[i].default_value);
    model_.segment_methods.push_back(state);
    if (model_.segment_method < 0)
        model_.segment_method = 0;
}

bool SegmentsDomain::Handles(IntentKind kind) const
{
    return kind == IntentKind::SetShowSegments || kind == IntentKind::SelectSegmentMethod ||
           kind == IntentKind::SetSegmentParam;
}

void SegmentsDomain::Handle(const Intent &intent, Intents &)
{
    switch (intent.kind) {
    case IntentKind::SetShowSegments:
        model_.show_segments = intent.i != 0;
        break;
    case IntentKind::SelectSegmentMethod:
        if (intent.i < 0 || intent.i >= static_cast<int>(model_.segment_methods.size()))
            return;
        model_.segment_method = intent.i;
        break;
    case IntentKind::SetSegmentParam: {
        if (model_.segment_method < 0)
            return;
        SegmentMethod &method = model_.segment_methods[static_cast<size_t>(model_.segment_method)];
        if (intent.i < 0 || intent.i >= static_cast<int>(method.values.size()))
            return;
        const or_filter_param &param = method.vtable->params[intent.i];
        method.values[static_cast<size_t>(intent.i)] = std::clamp(intent.x, param.min_value, param.max_value);
        break;
    }
    default:
        return;
    }
    Update();
}

void SegmentsDomain::Tick(Intents &) { Update(); }

void SegmentsDomain::Shutdown() { builder_.Stop(); }

// @impl 0f87-6acd-d9d9-da69
void SegmentsDomain::Update()
{
    if (!model_.show_segments || model_.tracks_loading || model_.tracks.empty() || model_.segment_method < 0) {
        model_.segments_building = builder_.Building();
        return;
    }
    // What the networks depend on: the map filter, the tracks loaded (their
    // version), the method and its parameter values (exactly: %a).
    const core::OutingFilter filter = model_.MapFilter();
    const SegmentMethod &method = model_.segment_methods[static_cast<size_t>(model_.segment_method)];
    std::string basis = filter.type_key + "|" + filter.from_date + "|" + filter.to_date + "|" +
                        std::to_string(model_.tracks_version) + "|" + method.vtable->id;
    for (double value : method.values) {
        char text[40];
        std::snprintf(text, sizeof text, "|%a", value);
        basis += text;
    }
    if (basis != basis_) {
        basis_ = basis;
        std::map<std::string, std::string> sport_of;  // outing id -> sport, for the shown outings
        for (size_t i : core::SelectOutings(model_.outings, filter, core::SortKey::Date, true))
            sport_of[model_.outings[i].raw.id] = model_.outings[i].type.sport;
        std::map<std::string, core::TrackGroup> groups;
        for (const openrider::sdk::OverviewTrack &track : model_.tracks) {
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
        builder_.Start(std::move(to_build), method.vtable, method.values);
        core::LogLine("building segment networks: %s", basis.c_str());
    }
    if (builder_.Take(model_.segment_groups)) {
        const std::string error = builder_.Error();
        if (!error.empty())
            ReportError(model_, "Segment identification failed: " + error);
        size_t shared = 0;
        for (const core::SegmentGroup &group : model_.segment_groups)
            for (const openrider::sdk::NetworkSegment &segment : group.network.segments)
                shared += segment.Shared() ? 1 : 0;
        char status[160];
        std::snprintf(status, sizeof status, "%zu shared segments in %zu activity groups", shared,
                      model_.segment_groups.size());
        model_.status = status;
        core::LogLine("%s", status);
    }
    model_.segments_building = builder_.Building();
    if (model_.segments_building)
        model_.status = "Finding common segments...";
}

} // namespace app
