// Graph controls shared by the outing and comparison panels: the x axis,
// adding a graph, and one row per graph (series, type, window/unit,
// remove).

#include "views.h"

namespace ui {

void DrawGraphControls(const std::vector<core::SeriesSpec> &graphs, core::XAxis xaxis, const GraphIntents &intents,
                       app::Intents &out)
{
    static const core::SeriesKind kinds[] = {core::SeriesKind::Altitude,    core::SeriesKind::Speed,
                                             core::SeriesKind::HeartRate,   core::SeriesKind::Cadence,
                                             core::SeriesKind::Temperature, core::SeriesKind::VerticalSpeed,
                                             core::SeriesKind::Grade};
    ImGui::TextUnformatted("X axis:");
    ImGui::SameLine();
    if (ImGui::RadioButton("Time", xaxis == core::XAxis::Time))
        out.Push(intents.set_xaxis, static_cast<int>(core::XAxis::Time));
    ImGui::SameLine();
    if (ImGui::RadioButton("Distance", xaxis == core::XAxis::Distance))
        out.Push(intents.set_xaxis, static_cast<int>(core::XAxis::Distance));
    ImGui::SameLine();
    if (ImGui::Button("+ Add graph"))
        out.Push(intents.add);

    for (size_t graph = 0; graph < graphs.size(); ++graph) {
        core::SeriesSpec spec = graphs[graph];  // edited locally, sent as an intent
        bool changed = false;
        ImGui::PushID(static_cast<int>(graph));
        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::BeginCombo("##series", core::SeriesLabel(spec.kind))) {
            for (core::SeriesKind kind : kinds)
                if (ImGui::Selectable(core::SeriesLabel(kind), spec.kind == kind)) {
                    spec.kind = kind;
                    changed = true;
                }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(95.0f);
        int type = spec.type == core::GraphType::Histogram ? 1 : 0;
        static const char *type_names[] = {"Curve", "Histogram"};
        if (ImGui::Combo("##type", &type, type_names, 2)) {
            spec.type = type == 1 ? core::GraphType::Histogram : core::GraphType::Curve;
            changed = true;
        }
        if (spec.kind == core::SeriesKind::VerticalSpeed) {
            static const double windows[] = {30.0, 60.0, 300.0};
            static const char *window_names[] = {"30 s", "1 min", "5 min"};
            static const core::VSpeedUnit units[] = {core::VSpeedUnit::MetresPerHour, core::VSpeedUnit::MetresPerMinute,
                                                     core::VSpeedUnit::MetresPerSecond};
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80.0f);
            int window = spec.vspeed_window_s == 30.0 ? 0 : spec.vspeed_window_s == 300.0 ? 2 : 1;
            if (ImGui::Combo("##window", &window, window_names, 3)) {
                spec.vspeed_window_s = windows[window];
                changed = true;
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80.0f);
            if (ImGui::BeginCombo("##unit", core::SeriesUnit(spec))) {
                for (core::VSpeedUnit unit : units) {
                    core::SeriesSpec unit_spec = spec;
                    unit_spec.vspeed_unit = unit;
                    if (ImGui::Selectable(core::SeriesUnit(unit_spec), spec.vspeed_unit == unit)) {
                        spec.vspeed_unit = unit;
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }
        } else if (spec.kind == core::SeriesKind::Grade) {
            static const char *window_names[] = {"100 m", "1000 m"};
            int window = spec.grade_window_m == 1000.0 ? 1 : 0;
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80.0f);
            if (ImGui::Combo("##gwindow", &window, window_names, 2)) {
                spec.grade_window_m = window == 1 ? 1000.0 : 100.0;
                changed = true;
            }
        }
        if (graphs.size() > 1) {
            ImGui::SameLine();
            if (ImGui::SmallButton("Remove"))
                out.Push(intents.remove, static_cast<int>(graph));
        }
        if (changed) {
            app::Intent intent(intents.set_spec);
            intent.i = static_cast<int>(graph);
            intent.spec = spec;
            out.Push(std::move(intent));
        }
        ImGui::PopID();
    }
}

} // namespace ui
