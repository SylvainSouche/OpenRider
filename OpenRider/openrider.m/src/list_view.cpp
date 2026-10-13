// Outing list view: query widgets, sort controls, three-line rows.

#include "views.h"

#include "core/outing_selection.h"
#include "core/overview.h"

#include <cfloat>
#include <cstring>
#include <set>

namespace ui {

namespace {

void DrawQuery(const app::AppModel &model, app::Intents &out)
{
    const app::OutingQuery &query = model.query;
    std::set<std::string> keys;
    for (const core::Outing &outing : model.outings)
        keys.insert(outing.type.Key());
    std::string preview = "All types";
    for (const core::Outing &outing : model.outings)
        if (outing.type.Key() == query.type_key) {
            preview = model.types.Label(outing.type, model.lang);
            break;
        }
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::BeginCombo("##type", preview.c_str())) {
        if (ImGui::Selectable("All types", query.type_key.empty()))
            out.PushText(app::IntentKind::SetTypeFilter, "");
        for (const std::string &k : keys) {
            core::ActivityType type;
            for (const core::Outing &outing : model.outings)
                if (outing.type.Key() == k) {
                    type = outing.type;
                    break;
                }
            if (ImGui::Selectable((model.types.Label(type, model.lang) + "##" + k).c_str(), query.type_key == k))
                out.PushText(app::IntentKind::SetTypeFilter, k);
        }
        ImGui::EndCombo();
    }
    // Dates: edited on a local copy of the model's value.
    const float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    char from[11], to[11];
    std::snprintf(from, sizeof from, "%s", query.from_date.c_str());
    std::snprintf(to, sizeof to, "%s", query.to_date.c_str());
    ImGui::SetNextItemWidth(half);
    if (ImGui::InputTextWithHint("##from", "from YYYY-MM-DD", from, sizeof from))
        out.PushText(app::IntentKind::SetDateFrom, from);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(half);
    if (ImGui::InputTextWithHint("##to", "to YYYY-MM-DD", to, sizeof to))
        out.PushText(app::IntentKind::SetDateTo, to);
    bool only_visible = query.only_visible;
    if (ImGui::Checkbox("Filter visible tracks", &only_visible))
        out.Push(app::IntentKind::SetOnlyVisible, only_visible ? 1 : 0);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Only outings whose track is at least partly within the overview map");
}

void DrawSort(const app::ListViewModel &view_model, app::Intents &out)
{
    static const char *names[] = {"Date", "Name", "Type", "Distance", "Duration", "Ascent"};
    static const core::SortKey keys[] = {core::SortKey::Date,     core::SortKey::Name,
                                         core::SortKey::Type,     core::SortKey::Distance,
                                         core::SortKey::Duration, core::SortKey::Ascent};
    int sort = 0;
    for (int k = 0; k < 6; ++k)
        if (keys[k] == view_model.sort_key)
            sort = k;
    ImGui::TextUnformatted("Sort:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110.0f);
    if (ImGui::Combo("##sort", &sort, names, 6))
        out.Push(app::IntentKind::SortBy, static_cast<int>(keys[sort]));
    ImGui::SameLine();
    if (ImGui::SmallButton(view_model.ascending ? "ascending" : "descending"))
        out.Push(app::IntentKind::ToggleSortDirection);
}

} // namespace

// @impl 0f87-6acd-24ce-ac37
void DrawListView(const app::AppModel &model, const ViewModels &view_models, app::Intents &out)
{
    DrawQuery(model, out);
    DrawSort(view_models.list, out);
    const std::vector<size_t> rows =
        core::SelectOutings(model.outings, model.ListFilter(), view_models.list.sort_key, view_models.list.ascending);
    ImGui::TextDisabled("%zu outings", rows.size());
    ImGui::Separator();

    // Rows: large icon in the activity's colour; long date and name; then
    // distance, duration (moving time) and D+.
    // @impl 0f87-6acd-5b42-dc4b
    // @impl 0f87-6acd-c813-9ed7
    ImGui::BeginChild("rows");
    const float line = ImGui::GetTextLineHeight();
    const float row_h = line * 3.4f;
    const ImU32 text = ImGui::GetColorU32(ImGuiCol_Text);
    const ImU32 dim = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    ImFont *font = ImGui::GetFont();
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(rows.size()), row_h + ImGui::GetStyle().ItemSpacing.y);
    while (clipper.Step()) {
        for (int r = clipper.DisplayStart; r < clipper.DisplayEnd; ++r) {
            const size_t i = rows[static_cast<size_t>(r)];
            const core::Outing &outing = model.outings[i];
            ImGui::PushID(static_cast<int>(i));
            const ImVec2 row_origin = ImGui::GetCursorScreenPos();
            const float w = ImGui::GetContentRegionAvail().x;
            const bool selected = model.selected == static_cast<int>(i);
            if (ImGui::Selectable("##row", selected, ImGuiSelectableFlags_None, ImVec2(0.0f, row_h)))
                out.Push(app::IntentKind::SelectOuting, selected ? -1 : static_cast<int>(i));  // again: overview
            ImDrawList *draw_list = ImGui::GetWindowDrawList();
            const ImVec4 clip(row_origin.x, row_origin.y, row_origin.x + w, row_origin.y + row_h);
            const std::string icon = model.types.Icon(outing.type);
            draw_list->AddText(font, row_h * 0.8f, ImVec2(row_origin.x + 4.0f, row_origin.y + row_h * 0.06f),
                        ColourU32(model.types.Colour(outing.type), 255), icon.c_str());
            const float x = row_origin.x + row_h + 6.0f;
            const std::string date = core::LongDate(outing.raw.start_local, model.lang);
            const float date_size = line * 1.3f;
            draw_list->AddText(font, date_size, ImVec2(x, row_origin.y + 3.0f), text, date.c_str());
            const float dw = font->CalcTextSizeA(date_size, FLT_MAX, 0.0f, date.c_str()).x;
            draw_list->AddText(font, line, ImVec2(x + dw + 10.0f, row_origin.y + 3.0f + (date_size - line) * 0.7f), text,
                        outing.raw.name, nullptr, 0.0f, &clip);
            const std::string info = "Distance: " + FormatNumber(outing.raw.distance_m / 1000.0, "%.1f km") +
                                     "   Duration: " + FormatDuration(outing.raw.duration_s) + " (" +
                                     FormatDuration(outing.raw.moving_s) + ")   D+ " + FormatNumber(outing.raw.ascent_m, "%.0f m");
            draw_list->AddText(font, line, ImVec2(x, row_origin.y + 6.0f + date_size), dim, info.c_str(), nullptr, 0.0f, &clip);
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
}

} // namespace ui
