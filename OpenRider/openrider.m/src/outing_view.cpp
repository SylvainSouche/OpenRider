// The right-hand panel: the overview (no outing selected) or the selected
// outing (statistics, filters, map, graphs).

#include "views.h"

#include <implot.h>

#include "core/long_date.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace ui {

namespace {

void DrawBackgroundCombo(const app::AppModel &model, const app::MapViewModel &view_model, app::Intents &out)
{
    ImGui::TextUnformatted("Background:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200.0f);
    const char *current = view_model.Layer() ? view_model.Layer()->name.c_str() : "-";
    if (ImGui::BeginCombo("##background", current)) {
        for (int i : app::MapOfferedLayers(view_model, model))
            if (ImGui::Selectable(view_model.layers[static_cast<size_t>(i)].name.c_str(), i == view_model.current))
                out.Push(app::IntentKind::SelectLayer, i);
        ImGui::EndCombo();
    }
}

// Garmin's statistics (Connect summary JSON, else GarminDB) by default;
// computed from the points when chosen, and always after a track filter.
// @impl 0f87-6acd-2cff-8135
void DrawStatistics(const app::AppModel &model, const app::OutingViewModel &view_model, app::Intents &out)
{
    const core::Outing &outing = *model.SelectedOuting();
    ImGui::Text("%s", outing.raw.name);
    ImGui::TextDisabled("%s  -  %s", outing.raw.start_local, model.types.Label(outing.type, model.lang).c_str());
    const bool filtered = model.AnyFilterOn();
    if (filtered) {
        ImGui::TextDisabled("Statistics computed from the filtered points");
    } else {
        ImGui::TextUnformatted("Statistics:");
        ImGui::SameLine();
        if (ImGui::RadioButton("Garmin", !view_model.stats_computed))
            out.Push(app::IntentKind::SetStatsComputed, 0);
        ImGui::SameLine();
        if (ImGui::RadioButton("Computed", view_model.stats_computed))
            out.Push(app::IntentKind::SetStatsComputed, 1);
    }
    ImGui::Separator();
    or_outing shown = outing.raw;
    double moving_speed = shown.moving_s > 0.0 ? shown.distance_m / shown.moving_s : std::numeric_limits<double>::quiet_NaN();
    if ((filtered || view_model.stats_computed) && !model.points.empty()) {
        const openrider::sdk::ComputedStats &computed = model.computed;
        shown.distance_m = computed.distance_m;
        shown.duration_s = computed.duration_s;
        shown.moving_s = computed.moving_s;
        shown.ascent_m = computed.ascent_m;
        shown.descent_m = computed.descent_m;
        shown.min_alt_m = computed.min_alt_m;
        shown.max_alt_m = computed.max_alt_m;
        shown.avg_speed_ms = computed.avg_speed_ms;
        shown.max_speed_ms = computed.max_speed_ms;
        moving_speed = computed.moving_speed_ms;
    }
    if (!ImGui::BeginTable("stats", 4, ImGuiTableFlags_SizingFixedFit))
        return;
    auto row = [](const char *label1, const std::string &value1, const char *label2, const std::string &value2) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextDisabled("%s", label1);
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(value1.c_str());
        ImGui::TableNextColumn();
        ImGui::TextDisabled("%s", label2);
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(value2.c_str());
    };
    row("Distance", FormatNumber(shown.distance_m / 1000.0, "%.2f km"), "Calories", FormatNumber(shown.calories, "%.0f"));
    row("Duration", FormatDuration(shown.duration_s), "Moving time", FormatDuration(shown.moving_s));
    row("Ascent", FormatNumber(shown.ascent_m, "%.0f m"), "Descent", FormatNumber(shown.descent_m, "%.0f m"));
    row("Min altitude", FormatNumber(shown.min_alt_m, "%.0f m"), "Max altitude", FormatNumber(shown.max_alt_m, "%.0f m"));
    row("Average speed", FormatNumber(shown.avg_speed_ms * 3.6, "%.1f km/h"), "Moving speed",
        FormatNumber(moving_speed * 3.6, "%.1f km/h"));
    row("Max speed", FormatNumber(shown.max_speed_ms * 3.6, "%.1f km/h"), "", "");
    row("Average HR", shown.avg_hr < 0 ? "-" : std::to_string(shown.avg_hr) + " bpm", "Max HR",
        shown.max_hr < 0 ? "-" : std::to_string(shown.max_hr) + " bpm");
    row("Temperature", FormatNumber(shown.avg_temp_c, "%.1f °C"), "Min / max",
        FormatNumber(shown.min_temp_c, "%.0f") + " / " + FormatNumber(shown.max_temp_c, "%.0f °C"));
    row("Aerobic TE", FormatNumber(shown.aerobic_te, "%.1f"), "Anaerobic TE", FormatNumber(shown.anaerobic_te, "%.1f"));
    row("Recorded as", std::string(shown.fit_sport) + " / " + shown.fit_sub_sport, "", "");
    ImGui::EndTable();
}

// One checkbox per filter plugin; parameters in a popup (local copies).
// @impl 0f87-6acd-33a9-64c0
void DrawFilterRow(const app::AppModel &model, app::Intents &out)
{
    if (model.filters.empty())
        return;
    ImGui::TextUnformatted("Filters:");
    for (size_t i = 0; i < model.filters.size(); ++i) {
        const app::FilterState &filter = model.filters[i];
        ImGui::PushID(static_cast<int>(i));
        ImGui::SameLine();
        bool enabled = filter.enabled;
        if (ImGui::Checkbox(filter.vtable->label, &enabled))
            out.Push(app::IntentKind::ToggleFilter, static_cast<int>(i), enabled ? 1 : 0);
        ImGui::SameLine();
        if (ImGui::SmallButton("..."))
            ImGui::OpenPopup("params");
        if (ImGui::BeginPopup("params")) {
            ImGui::TextUnformatted(filter.vtable->label);
            ImGui::Separator();
            for (size_t k = 0; k < filter.vtable->param_count; ++k) {
                const or_filter_param &param = filter.vtable->params[k];
                double value = filter.values[k];
                ImGui::SetNextItemWidth(160.0f);
                if (ImGui::SliderScalar(param.label, ImGuiDataType_Double, &value, &param.min_value, &param.max_value, "%.2f"))
                    out.Push(app::IntentKind::SetFilterParam, static_cast<int>(i), static_cast<int>(k), value);
            }
            if (ImGui::Button("Defaults"))
                out.Push(app::IntentKind::ResetFilterParams, static_cast<int>(i));
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
}

void DrawTrackStyle(const app::MapViewModel &view_model, app::Intents &out)
{
    ImGui::TextUnformatted("Track:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150.0f);
    if (ImGui::BeginCombo("##trackstyle", app::kTrackStyles[view_model.track_style].name)) {
        for (int i = 0; i < 6; ++i)
            if (ImGui::Selectable(app::kTrackStyles[i].name, view_model.track_style == i))
                out.Push(app::IntentKind::SetTrackStyle, i);
        ImGui::EndCombo();
    }
    if (view_model.track_style == 0) {
        ImGui::SameLine();
        ImVec4 colour = ImGui::ColorConvertU32ToFloat4(view_model.track_colour);
        if (ImGui::ColorEdit4("##trackcolour", &colour.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar))
            out.Push(app::IntentKind::SetTrackColour, 0, 0, static_cast<double>(ImGui::ColorConvertFloat4ToU32(colour)));
    }
}


// Stacked graphs sharing the x axis; the value under the mouse in a
// tooltip, a cursor line across all graphs.
// @impl 0f87-6acd-2d65-0c79
// @impl 0f87-6acd-32ed-0184
void DrawGraphs(const app::AppModel &model, const app::OutingViewModel &view_model, float height, app::Intents &out)
{
    if (model.points.empty()) {
        ImGui::TextDisabled("No recorded points");
        return;
    }
    const int rows = static_cast<int>(view_model.graphs.size());
    if (!ImPlot::BeginSubplots("##graphs", rows, 1, ImVec2(-1.0f, height),
                               ImPlotSubplotFlags_LinkAllX | ImPlotSubplotFlags_NoTitle))
        return;
    const char *xlabel = view_model.xaxis == openrider::sdk::XAxis::Time ? "time (min)" : "distance (km)";
    bool any_hover = false;
    for (int graph = 0; graph < rows; ++graph) {
        const openrider::sdk::SeriesSpec &spec = view_model.graphs[static_cast<size_t>(graph)];
        const openrider::sdk::Series series = app::GraphSeries(model.points, view_model.xaxis, spec);
        char ylabel[64];
        std::snprintf(ylabel, sizeof ylabel, "%s (%s)", openrider::sdk::SeriesLabel(spec.kind), openrider::sdk::SeriesUnit(spec));
        char id[16];
        std::snprintf(id, sizeof id, "##g%d", graph);
        if (!ImPlot::BeginPlot(id, ImVec2(-1.0f, 0.0f), ImPlotFlags_NoLegend))
            continue;
        ImPlot::SetupAxes(graph == rows - 1 ? xlabel : nullptr, ylabel, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
        if (spec.type == openrider::sdk::GraphType::Histogram) {
            ImPlotSpec bars;
            bars.Flags = ImPlotStairsFlags_Shaded;
            bars.FillAlpha = 0.5f;
            ImPlot::PlotStairs(ylabel, series.x.data(), series.y.data(), static_cast<int>(series.x.size()), bars);
        } else {
            ImPlot::PlotLine(ylabel, series.x.data(), series.y.data(), static_cast<int>(series.x.size()));
        }
        if (view_model.hover_index >= 0) {
            ImPlotSpec cursor;
            cursor.LineColor = ImVec4(0.2f, 0.2f, 0.2f, 0.6f);
            ImPlot::PlotInfLines("##cursor", &view_model.hover_x, 1, cursor);
        }
        if (ImPlot::IsPlotHovered()) {
            any_hover = true;
            app::Intent hover(app::IntentKind::GraphHover);
            hover.i = graph;
            hover.x = ImPlot::GetPlotMousePos().x;
            out.Push(std::move(hover));
            // Tooltip: the value at the hovered sample of this graph.
            const double hover_x = view_model.hover_x;
            size_t nearest_sample = 0;
            double nearest_distance = std::numeric_limits<double>::infinity();
            for (size_t k = 0; k < series.x.size(); ++k)
                if (std::fabs(series.x[k] - hover_x) < nearest_distance) {
                    nearest_distance = std::fabs(series.x[k] - hover_x);
                    nearest_sample = k;
                }
            if (view_model.hover_index >= 0 && nearest_sample < series.y.size()) {
                ImGui::BeginTooltip();
                ImGui::Text("%s  %.2f", xlabel, series.x[nearest_sample]);
                if (std::isnan(series.y[nearest_sample]))
                    ImGui::Text("%s  -", ylabel);
                else
                    ImGui::Text("%s  %.1f", ylabel, series.y[nearest_sample]);
                ImGui::EndTooltip();
            }
        }
        ImPlot::EndPlot();
    }
    ImPlot::EndSubplots();
    if (!any_hover && view_model.hover_index >= 0)
        out.Push(app::IntentKind::GraphHoverEnd);
}

// The common segments switch, the method (when several segments plugins
// are installed) and one slider per parameter the method declares. Values
// are edited on local copies; every change is a new build request.
void DrawSegmentControls(const app::AppModel &model, app::Intents &out)
{
    if (model.segment_methods.empty()) {
        ImGui::TextDisabled("(no segments plugin)");
        return;
    }
    bool show_segments = model.show_segments;
    if (ImGui::Checkbox("Common segments", &show_segments))
        out.Push(app::IntentKind::SetShowSegments, show_segments ? 1 : 0);
    if (!model.show_segments || model.segment_method < 0)
        return;
    const app::SegmentMethod &method = model.segment_methods[static_cast<size_t>(model.segment_method)];
    if (model.segment_methods.size() > 1) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(180.0f);
        if (ImGui::BeginCombo("##segment_method", method.vtable->label)) {
            for (size_t m = 0; m < model.segment_methods.size(); ++m)
                if (ImGui::Selectable(model.segment_methods[m].vtable->label, static_cast<int>(m) == model.segment_method))
                    out.Push(app::IntentKind::SelectSegmentMethod, static_cast<int>(m));
            ImGui::EndCombo();
        }
    }
    for (size_t p = 0; p < method.values.size(); ++p) {
        const or_filter_param &param = method.vtable->params[p];
        ImGui::SameLine();
        ImGui::SetNextItemWidth(140.0f);
        ImGui::PushID(static_cast<int>(p));
        double value = method.values[p];
        if (ImGui::SliderScalar(param.label, ImGuiDataType_Double, &value, &param.min_value, &param.max_value, "%.1f"))
            out.Push(app::IntentKind::SetSegmentParam, static_cast<int>(p), 0, value);
        ImGui::PopID();
    }
}

// Overview with segments: what the hovered segment is and which outings
// follow it, newest first.
void DrawSegmentTooltip(const app::AppModel &model, const app::MapViewModel &view_model)
{
    if (view_model.hovered_group < 0 || view_model.hovered_group >= static_cast<int>(model.segment_groups.size()))
        return;
    const core::SegmentGroup &group = model.segment_groups[static_cast<size_t>(view_model.hovered_group)];
    if (view_model.hovered_segment < 0 || view_model.hovered_segment >= static_cast<int>(group.network.segments.size()))
        return;
    const openrider::sdk::NetworkSegment &segment = group.network.segments[static_cast<size_t>(view_model.hovered_segment)];
    std::vector<const core::Outing *> routes;
    for (const std::string &id : segment.route_ids)
        for (const core::Outing &outing : model.outings)
            if (outing.raw.id == id)
                routes.push_back(&outing);
    std::sort(routes.begin(), routes.end(), [](const core::Outing *a, const core::Outing *b) {
        return std::strcmp(a->raw.start_local, b->raw.start_local) > 0;
    });
    ImGui::BeginTooltip();
    ImGui::Text("Segment %d (%s)  -  %.2f km  -  %zu route%s", segment.id, group.sport.c_str(), segment.length_m / 1000.0,
                routes.size(), routes.size() == 1 ? "" : "s");
    ImGui::TextDisabled("nodes %d -> %d", segment.from, segment.to);
    ImGui::Separator();
    const size_t max_listed = 15;
    for (size_t i = 0; i < routes.size() && i < max_listed; ++i)
        ImGui::Text("%s  %s", core::LongDate(routes[i]->raw.start_local, model.lang).c_str(), routes[i]->raw.name);
    if (routes.size() > max_listed)
        ImGui::TextDisabled("... and %zu more", routes.size() - max_listed);
    ImGui::EndTooltip();
}

} // namespace

// No outing selected: every listed track on the map; click one to open it.
void DrawOverviewView(const app::AppModel &model, const ViewModels &view_models, TextureMirror &textures, app::Intents &out)
{
    ImGui::Text("All outings  -  %zu tracks loaded", model.tracks.size());
    ImGui::SameLine();
    DrawBackgroundCombo(model, view_models.map, out);
    ImGui::SameLine();
    DrawSegmentControls(model, out);
    if (model.show_segments && model.segments_building) {
        ImGui::SameLine();
        ImGui::TextDisabled("computing...");
    }
    // The same track filters as for an outing, applied to every track shown.
    DrawFilterRow(model, out);
    if (!model.filters.empty() && model.tracks_loading) {
        ImGui::SameLine();
        ImGui::TextDisabled("loading tracks...");
    }
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    DrawMapView(model, view_models, textures, avail.x, avail.y, out);
    DrawSegmentTooltip(model, view_models.map);
    if (!view_models.map.hovered_track.empty())
        for (const core::Outing &outing : model.outings)
            if (view_models.map.hovered_track == outing.raw.id)
                ImGui::SetTooltip("%s\n%s", core::LongDate(outing.raw.start_local, model.lang).c_str(), outing.raw.name);
}

void DrawOutingView(const app::AppModel &model, const ViewModels &view_models, TextureMirror &textures, app::Intents &out)
{
    if (ImGui::SmallButton("< All outings"))
        out.Push(app::IntentKind::SelectOuting, -1);
    ImGui::SameLine();
    if (ImGui::CollapsingHeader("Statistics", ImGuiTreeNodeFlags_DefaultOpen))
        DrawStatistics(model, view_models.outing, out);
    ImGui::Separator();
    DrawFilterRow(model, out);
    DrawBackgroundCombo(model, view_models.map, out);
    ImGui::SameLine();
    DrawTrackStyle(view_models.map, out);

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float map_height = std::clamp(view_models.main.map_height > 0.0f ? view_models.main.map_height : avail.y * 0.45f, 120.0f,
                               std::max(120.0f, avail.y - 160.0f));
    DrawMapView(model, view_models, textures, avail.x, map_height, out);
    const float resized = Splitter("##split_map", false, avail.x, map_height);
    if (resized >= 0.0f)
        out.Push(app::IntentKind::ResizeMap, 0, 0, 0.0, resized);
    DrawGraphControls(view_models.outing.graphs, view_models.outing.xaxis,
                      {app::IntentKind::SetXAxis, app::IntentKind::AddGraph, app::IntentKind::RemoveGraph,
                       app::IntentKind::SetGraphSpec},
                      out);
    DrawGraphs(model, view_models.outing, ImGui::GetContentRegionAvail().y, out);
}

} // namespace ui
