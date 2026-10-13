#include "app/outing_domain.h"

#include "app/reports.h"

#include "core/log.h"
#include "core/plugin_thread.h"

namespace app {

void OutingDomain::AddFilter(const or_filter_vtable *filter)
{
    FilterState state;
    state.vtable = filter;
    for (size_t i = 0; i < filter->param_count; ++i)
        state.values.push_back(filter->params[i].default_value);
    model_.filters.push_back(state);
}

bool OutingDomain::Handles(IntentKind kind) const
{
    return kind == IntentKind::SelectOuting || kind == IntentKind::ToggleFilter || kind == IntentKind::SetFilterParam ||
           kind == IntentKind::ResetFilterParams;
}

void OutingDomain::Handle(const Intent &intent, Intents &out)
{
    const int filter_count = static_cast<int>(model_.filters.size());
    switch (intent.kind) {
    case IntentKind::SelectOuting:
        if (intent.i >= 0 && model_.comparison.open)
            out.Push(IntentKind::CloseSegmentComparison);  // the panel shows the outing
        Select(intent.i);
        break;
    case IntentKind::ToggleFilter:
        if (intent.i >= 0 && intent.i < filter_count) {
            model_.filters[static_cast<size_t>(intent.i)].enabled = intent.j != 0;
            ApplyFilters();
        }
        break;
    case IntentKind::SetFilterParam:
        if (intent.i >= 0 && intent.i < filter_count) {
            FilterState &filter = model_.filters[static_cast<size_t>(intent.i)];
            if (intent.j >= 0 && intent.j < static_cast<int>(filter.values.size())) {
                filter.values[static_cast<size_t>(intent.j)] = intent.x;
                if (filter.enabled)
                    ApplyFilters();
            }
        }
        break;
    case IntentKind::ResetFilterParams:
        if (intent.i >= 0 && intent.i < filter_count) {
            FilterState &filter = model_.filters[static_cast<size_t>(intent.i)];
            for (size_t k = 0; k < filter.values.size(); ++k)
                filter.values[k] = filter.vtable->params[k].default_value;
            if (filter.enabled)
                ApplyFilters();
        }
        break;
    default:
        break;
    }
}

void OutingDomain::Select(int index)
{
    if (index >= static_cast<int>(model_.outings.size()))
        index = -1;
    model_.selected = index;
    const core::Outing *outing = model_.SelectedOuting();
    if (!outing || !input_.Ready() || model_.points_id == outing->raw.id)
        return;
    model_.points_id = outing->raw.id;
    const openrider::sdk::Status loaded = input_.LoadPoints(outing->raw.id, model_.raw_points);
    if (!loaded.Ok())
        ReportError(model_, std::string("Cannot load points of ") + outing->raw.name + ": " + loaded.error);
    core::LogLine("loaded %zu points for %s", model_.raw_points.size(), outing->raw.id);
    ApplyFilters();
}

void OutingDomain::ApplyFilters()
{
    model_.points = model_.raw_points;
    // Each filter marks the points to keep; the next filter sees only those.
    for (const FilterState &filter : model_.filters) {
        if (!filter.enabled || filter.vtable->kind != OR_FILTER_TRACK || model_.points.empty())
            continue;
        std::vector<unsigned char> keep(model_.points.size(), 1);
        char error[256] = "";
        int rc = 0;
        core::CallPlugin(filter.vtable, [&] {
            rc = filter.vtable->apply_track(model_.points.data(), model_.points.size(), filter.values.data(),
                                            keep.data(), error, sizeof error);
        });
        if (rc != 0) {
            ReportError(model_, std::string("Filter ") + filter.vtable->label + " failed: " + error);
            continue;
        }
        std::vector<or_point> kept;
        for (size_t i = 0; i < model_.points.size(); ++i)
            if (keep[i])
                kept.push_back(model_.points[i]);
        core::LogLine("filter %s kept %zu of %zu points", filter.vtable->id, kept.size(), model_.points.size());
        model_.points.swap(kept);
    }
    // Moving time from the raw points (dec-moving-time-from-raw).
    model_.computed = openrider::sdk::ComputeStats(model_.points, openrider::sdk::MotionParams{}, &model_.raw_points);
}

} // namespace app
