// Graph controls shared by the outing and comparison panels: the x axis,
// adding a graph, and one row per graph (series, type, window/unit,
// remove).

#include "views.h"

namespace ui {

void DrawGraphControls(const std::vector<openrider::sdk::SeriesSpec> &graphs, openrider::sdk::XAxis xaxis, const GraphIntents &intents,
                       app::Intents &out, bool timed)
{
    static const openrider::sdk::SeriesKind kinds[] = {openrider::sdk::SeriesKind::Altitude,    openrider::sdk::SeriesKind::Speed,
                                             openrider::sdk::SeriesKind::HeartRate,   openrider::sdk::SeriesKind::Cadence,
                                             openrider::sdk::SeriesKind::Temperature, openrider::sdk::SeriesKind::VerticalSpeed,
                                             openrider::sdk::SeriesKind::Grade};
    ImGui::TextUnformatted("X axis:");
    ImGui::SameLine();
    // Without times (a route), the time axis is not offered.
    ImGui::BeginDisabled(!timed);
    const bool time_clicked = ImGui::RadioButton("Time", xaxis == openrider::sdk::XAxis::Time);
    ImGui::EndDisabled();
    if (!timed && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("No times in this outing (a route)");
    if (time_clicked)
        out.Push(intents.set_xaxis, static_cast<int>(openrider::sdk::XAxis::Time));
    ImGui::SameLine();
    if (ImGui::RadioButton("Distance", xaxis == openrider::sdk::XAxis::Distance))
        out.Push(intents.set_xaxis, static_cast<int>(openrider::sdk::XAxis::Distance));
    ImGui::SameLine();
    if (ImGui::Button("+ Add graph"))
        out.Push(intents.add);

    for (size_t graph = 0; graph < graphs.size(); ++graph) {
        openrider::sdk::SeriesSpec spec = graphs[graph];  // edited locally, sent as an intent
        bool changed = false;
        ImGui::PushID(static_cast<int>(graph));
        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::BeginCombo("##series", openrider::sdk::SeriesLabel(spec.kind))) {
            for (openrider::sdk::SeriesKind kind : kinds)
                if (ImGui::Selectable(openrider::sdk::SeriesLabel(kind), spec.kind == kind)) {
                    spec.kind = kind;
                    changed = true;
                }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(95.0f);
        int type = spec.type == openrider::sdk::GraphType::Histogram ? 1 : 0;
        static const char *type_names[] = {"Curve", "Histogram"};
        if (ImGui::Combo("##type", &type, type_names, 2)) {
            spec.type = type == 1 ? openrider::sdk::GraphType::Histogram : openrider::sdk::GraphType::Curve;
            changed = true;
        }
        if (spec.kind == openrider::sdk::SeriesKind::VerticalSpeed) {
            static const double windows[] = {30.0, 60.0, 300.0};
            static const char *window_names[] = {"30 s", "1 min", "5 min"};
            static const openrider::sdk::VSpeedUnit units[] = {openrider::sdk::VSpeedUnit::MetresPerHour, openrider::sdk::VSpeedUnit::MetresPerMinute,
                                                     openrider::sdk::VSpeedUnit::MetresPerSecond};
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80.0f);
            int window = spec.vspeed_window_s == 30.0 ? 0 : spec.vspeed_window_s == 300.0 ? 2 : 1;
            if (ImGui::Combo("##window", &window, window_names, 3)) {
                spec.vspeed_window_s = windows[window];
                changed = true;
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80.0f);
            if (ImGui::BeginCombo("##unit", openrider::sdk::SeriesUnit(spec))) {
                for (openrider::sdk::VSpeedUnit unit : units) {
                    openrider::sdk::SeriesSpec unit_spec = spec;
                    unit_spec.vspeed_unit = unit;
                    if (ImGui::Selectable(openrider::sdk::SeriesUnit(unit_spec), spec.vspeed_unit == unit)) {
                        spec.vspeed_unit = unit;
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }
        } else if (spec.kind == openrider::sdk::SeriesKind::Grade) {
            static const double windows[] = {100.0, 500.0, 1000.0};
            static const char *window_names[] = {"100 m", "500 m", "1000 m"};
            int window = spec.grade_window_m == 1000.0 ? 2 : spec.grade_window_m == 500.0 ? 1 : 0;
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80.0f);
            if (ImGui::Combo("##gwindow", &window, window_names, 3)) {
                spec.grade_window_m = windows[window];
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
