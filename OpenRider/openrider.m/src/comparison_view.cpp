// Segment comparison panel: the passages along a segment, newest first,
// with a check box each, beside the map; below, stacked graphs with one
// curve per shown passage, in the passage's colour.

#include "views.h"

#include <implot.h>

#include "core/overview.h"

#include <cmath>
#include <cstdio>

namespace ui {

namespace {

// "h:mm:ss" without the hours when zero.
std::string ShortDuration(double seconds)
{
    const int total = static_cast<int>(seconds + 0.5);
    char text[32];
    if (total >= 3600)
        std::snprintf(text, sizeof text, "%d:%02d:%02d", total / 3600, (total / 60) % 60, total % 60);
    else
        std::snprintf(text, sizeof text, "%d:%02d", total / 60, total % 60);
    return text;
}

// One line per passage: its colour, a check box, the date, the time it
// took; a second passage of the same outing says so, and a passage against
// the segment's direction is marked with an arrow.
void DrawPassageList(const app::AppModel &model, const app::ComparisonViewModel &view_model, app::Intents &out)
{
    if (ImGui::SmallButton("All"))
        out.Push(app::IntentKind::ShowAllComparedRuns, 1);
    ImGui::SameLine();
    if (ImGui::SmallButton("None"))
        out.Push(app::IntentKind::ShowAllComparedRuns, 0);
    ImGui::Separator();
    for (size_t r = 0; r < model.comparison.runs.size(); ++r) {
        const app::ComparedRun &run = model.comparison.runs[r];
        const core::Outing &outing = model.outings[run.outing];
        ImGui::PushID(static_cast<int>(r));
        bool shown = r < view_model.shown.size() && view_model.shown[r];
        ImGui::PushStyleColor(ImGuiCol_CheckMark, RunColour(r));
        if (ImGui::Checkbox("##shown", &shown))
            out.Push(app::IntentKind::ToggleComparedRun, static_cast<int>(r));
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::ColorButton("##colour", RunColour(r), ImGuiColorEditFlags_NoTooltip, ImVec2(10, 10));
        ImGui::SameLine();
        ImGui::Text("%s%s  %s %s", core::LongDate(outing.raw.start_local, model.lang).c_str(),
                    run.pass > 1 ? (" (" + std::to_string(run.pass) + ")").c_str() : "", ShortDuration(run.duration_s).c_str(),
                    run.forward ? "" : "<-");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", outing.raw.name);
        ImGui::PopID();
    }
}

// Stacked graphs sharing the x axis: per graph, one curve (or stairs) per
// shown passage. On the distance axis a passage is placed by its position
// along the segment, so passages in both directions line up on the ground;
// on the time axis it starts at its entry into the segment.
void DrawComparisonGraphs(const app::AppModel &model, const app::ComparisonViewModel &view_model, float height)
{
    const int rows = static_cast<int>(view_model.graphs.size());
    if (!ImPlot::BeginSubplots("##comparison_graphs", rows, 1, ImVec2(-1.0f, height),
                               ImPlotSubplotFlags_LinkAllX | ImPlotSubplotFlags_NoTitle))
        return;
    const bool distance = view_model.xaxis == core::XAxis::Distance;
    const char *xlabel = distance ? "along the segment (km)" : "time from entry (min)";
    for (int g = 0; g < rows; ++g) {
        const core::SeriesSpec &spec = view_model.graphs[static_cast<size_t>(g)];
        char ylabel[64];
        std::snprintf(ylabel, sizeof ylabel, "%s (%s)", core::SeriesLabel(spec.kind), core::SeriesUnit(spec));
        char id[24];
        std::snprintf(id, sizeof id, "##comparison%d", g);
        if (!ImPlot::BeginPlot(id, ImVec2(-1.0f, 0.0f), g == 0 ? ImPlotFlags_None : ImPlotFlags_NoLegend))
            continue;
        ImPlot::SetupAxes(g == rows - 1 ? xlabel : nullptr, ylabel, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
        for (size_t r = 0; r < model.comparison.runs.size(); ++r) {
            if (r >= view_model.shown.size() || !view_model.shown[r])
                continue;
            const app::ComparedRun &run = model.comparison.runs[r];
            char label[64];
            std::snprintf(label, sizeof label, "%s%s", core::LongDate(model.outings[run.outing].raw.start_local, model.lang).c_str(),
                          run.pass > 1 ? (" (" + std::to_string(run.pass) + ")").c_str() : "");
            ImPlotSpec style;
            style.LineColor = RunColour(r);
            if (spec.type == core::GraphType::Histogram) {
                const core::Series series = app::GraphSeries(run.points, view_model.xaxis, spec);
                style.Flags = ImPlotStairsFlags_None;
                ImPlot::PlotStairs(label, series.x.data(), series.y.data(), static_cast<int>(series.x.size()), style);
                continue;
            }
            const core::Series series = app::GraphSeries(run.points, view_model.xaxis, spec);
            std::vector<double> x(series.y.size());
            for (size_t k = 0; k < series.y.size(); ++k) {
                const size_t point = k < series.point_index.size() ? series.point_index[k] : k;
                x[k] = distance ? run.along_m[std::min(point, run.along_m.size() - 1)] / 1000.0 : series.x[k];
            }
            ImPlot::PlotLine(label, x.data(), series.y.data(), static_cast<int>(x.size()), style);
        }
        ImPlot::EndPlot();
    }
    ImPlot::EndSubplots();
}

} // namespace

// @impl 0f87-6ace-35a8-5c96
void DrawComparisonView(const app::AppModel &model, const ViewModels &view_models, TextureMirror &textures, app::Intents &out)
{
    const app::SegmentComparison &comparison = model.comparison;
    if (ImGui::SmallButton("< Back"))
        out.Push(app::IntentKind::CloseSegmentComparison);
    ImGui::SameLine();
    ImGui::Text("Segment (%s)  -  %.2f km  -  %zu passages", comparison.sport.c_str(), comparison.segment.length_m / 1000.0,
                comparison.runs.size());

    // First row: the passages, and the map.
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float row_height = std::clamp(view_models.main.map_height > 0.0f ? view_models.main.map_height : avail.y * 0.45f,
                                        120.0f, std::max(120.0f, avail.y - 160.0f));
    const float list_width = std::min(300.0f, avail.x * 0.35f);
    ImGui::BeginChild("passages", ImVec2(list_width, row_height), ImGuiChildFlags_Borders);
    DrawPassageList(model, view_models.comparison, out);
    ImGui::EndChild();
    ImGui::SameLine();
    DrawMapView(model, view_models, textures, ImGui::GetContentRegionAvail().x, row_height, out);
    const float resized = Splitter("##split_comparison", false, avail.x, row_height);
    if (resized >= 0.0f)
        out.Push(app::IntentKind::ResizeMap, 0, 0, 0.0, resized);

    // Below: the graphs.
    DrawGraphControls(view_models.comparison.graphs, view_models.comparison.xaxis,
                      {app::IntentKind::SetComparisonXAxis, app::IntentKind::AddComparisonGraph,
                       app::IntentKind::RemoveComparisonGraph, app::IntentKind::SetComparisonGraphSpec},
                      out);
    DrawComparisonGraphs(model, view_models.comparison, ImGui::GetContentRegionAvail().y);
}

} // namespace ui
