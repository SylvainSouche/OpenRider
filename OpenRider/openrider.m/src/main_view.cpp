// Main view: window, menu bar, the list/panel split, status bar, dialogs.

#include "views.h"

#include <algorithm>
#include <string>
#include <vector>

namespace ui {

namespace {

void DrawMenuBar(app::Intents &out)
{
    if (!ImGui::BeginMenuBar())
        return;
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Quit", "Cmd+Q"))
            out.Push(app::IntentKind::Quit);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("About OpenRider"))
            out.Push(app::IntentKind::ShowAbout, 1);
        ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
}

// The current action, progress for long tasks; tile errors here only.
// @impl 0f87-6acd-23b0-7c7d
void DrawStatusBar(const app::AppModel &model, const app::MapViewModel &map)
{
    ImGui::Separator();
    std::string status = model.status;
    if (map.tile_errors > 0)
        status += "  |  map: " + std::to_string(map.tile_errors) + " tile errors";
    if (map.tiles_pending > 0)
        status += "  |  loading " + std::to_string(map.tiles_pending) + " tiles";
    ImGui::TextUnformatted(status.c_str());
    float progress = -1.0f;
    if (model.scanning)
        progress = model.scan_total ? static_cast<float>(model.scanned) / static_cast<float>(model.scan_total) : 0.0f;
    else if (model.tracks_loading)
        progress = model.tracks_total ? static_cast<float>(model.tracks_done) / static_cast<float>(model.tracks_total) : 0.0f;
    if (progress >= 0.0f) {
        ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.6f);
        ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f));
    }
}

void DrawAbout(const app::MainViewModel &view_model, app::Intents &out)
{
    if (view_model.show_about && !ImGui::IsPopupOpen("About OpenRider"))
        ImGui::OpenPopup("About OpenRider");
    if (!ImGui::BeginPopupModal("About OpenRider", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;
    ImGui::TextUnformatted("OpenRider -- manage and display outings");
    ImGui::TextUnformatted("BSD-3-Clause, (c) 2026 Sylvain Souche");
    ImGui::Separator();
    ImGui::TextUnformatted("Credits:");
    ImGui::BulletText("Dear ImGui (MIT) -- Omar Cornut and contributors");
    ImGui::BulletText("ImPlot (MIT) -- Evan Pezent");
    ImGui::BulletText("GLFW (zlib) -- Marcus Geelnard, Camilla Loewy");
    ImGui::BulletText("cJSON (MIT) -- Dave Gamble and contributors");
    ImGui::BulletText("SQLite (public domain)");
    ImGui::BulletText("stb_image (public domain / MIT) -- Sean Barrett");
    ImGui::BulletText("libcurl (curl licence) -- Daniel Stenberg and contributors");
    ImGui::BulletText("libtiff (libtiff licence) -- Sam Leffler, Silicon Graphics");
    ImGui::BulletText("Phosphor icons (MIT) -- Phosphor Icons");
    ImGui::BulletText("Data: GarminDB output (read only)");
    ImGui::BulletText("Maps: OpenTopoMap (CC-BY-SA), map data OpenStreetMap contributors, SRTM;");
    ImGui::BulletText("      IGN SCAN 25 Touristique (Geoplateforme)");
    if (ImGui::Button("Close")) {
        out.Push(app::IntentKind::ShowAbout, 0);
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

// New outings whose type is to be confirmed (GPX files): each with its
// category and subcategory, prefilled from the file, changed or confirmed.
void DrawTypeConfirmation(const app::AppModel &model, app::Intents &out)
{
    std::vector<size_t> pending;
    for (size_t i = 0; i < model.outings.size(); ++i)
        if (!model.outings[i].confirmed)
            pending.push_back(i);
    if (pending.empty() || model.scanning || !model.errors.empty())
        return;
    const char *title = "New outings: confirm their type";
    if (!ImGui::IsPopupOpen(title))
        ImGui::OpenPopup(title);
    if (!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;
    auto set_type = [&](size_t outing, const std::string &key) {
        app::Intent intent{app::IntentKind::SetOutingType};
        intent.i = static_cast<int>(outing);
        intent.text = key;
        out.Push(std::move(intent));
    };
    ImGui::Text("%zu new outing%s from GPX files. Check each type, then confirm.", pending.size(),
                pending.size() == 1 ? "" : "s");
    // The categories (sports) and their subcategories with a label.
    std::vector<std::string> sports;
    std::vector<std::string> keys = model.types.LabelledKeys();
    for (const std::string &key : keys)
        if (key.find('/') == std::string::npos)
            sports.push_back(key);
    const float height = std::min(400.0f, ImGui::GetTextLineHeightWithSpacing() * (pending.size() + 1.5f));
    if (ImGui::BeginTable("##pending", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit,
                          ImVec2(760.0f, height))) {
        ImGui::TableSetupColumn("Date");
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("km");
        ImGui::TableSetupColumn("Category");
        ImGui::TableSetupColumn("Subcategory");
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();
        for (size_t i : pending) {
            const core::Outing &outing = model.outings[i];
            ImGui::PushID(static_cast<int>(i));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(std::string(outing.raw.start_local).substr(0, 16).c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(outing.raw.name);
            ImGui::TableNextColumn();
            ImGui::Text("%.1f", outing.raw.distance_m / 1000.0);
            ImGui::TableNextColumn();
            const std::string &sport = outing.type.sport;
            ImGui::SetNextItemWidth(170.0f);
            if (ImGui::BeginCombo("##sport", model.types.KeyLabel(sport, model.lang).c_str())) {
                for (const std::string &choice : sports)
                    if (ImGui::Selectable(model.types.KeyLabel(choice, model.lang).c_str(), choice == sport))
                        set_type(i, choice + "/generic");
                ImGui::EndCombo();
            }
            ImGui::TableNextColumn();
            const std::string current = sport + "/" + outing.type.sub_sport;
            ImGui::SetNextItemWidth(170.0f);
            const std::string preview = outing.type.sub_sport == "generic" ? "-" : model.types.KeyLabel(current, model.lang);
            if (ImGui::BeginCombo("##sub", preview.c_str())) {
                if (ImGui::Selectable("-", outing.type.sub_sport == "generic"))
                    set_type(i, sport + "/generic");
                for (const std::string &choice : keys)
                    if (choice.rfind(sport + "/", 0) == 0 &&
                        ImGui::Selectable(model.types.KeyLabel(choice, model.lang).c_str(), choice == current))
                        set_type(i, choice);
                ImGui::EndCombo();
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (ImGui::Button("Confirm")) {
        out.Push(app::IntentKind::ConfirmOutingTypes);
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void DrawErrorPopup(const app::AppModel &model, app::Intents &out)
{
    if (model.errors.empty())
        return;
    if (!ImGui::IsPopupOpen("Error"))
        ImGui::OpenPopup("Error");
    if (!ImGui::BeginPopupModal("Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;
    ImGui::PushTextWrapPos(500.0f);
    ImGui::TextUnformatted(model.errors.front().c_str());
    ImGui::PopTextWrapPos();
    if (ImGui::Button("OK")) {
        out.Push(app::IntentKind::DismissError);
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

} // namespace

void DrawMainView(const app::AppModel &model, const ViewModels &view_models, TextureMirror &textures, app::Intents &out)
{
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::Begin("OpenRider", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_MenuBar);
    DrawMenuBar(out);

    const float status_h = ImGui::GetFrameHeightWithSpacing() + 4.0f;
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float body_h = avail.y - status_h;
    const float list_w = std::clamp(view_models.main.list_width, 200.0f, std::max(200.0f, avail.x - 300.0f));

    ImGui::BeginChild("outings", ImVec2(list_w, body_h), ImGuiChildFlags_Borders);
    DrawListView(model, view_models, out);
    ImGui::EndChild();
    ImGui::SameLine(0.0f, 0.0f);
    const float resized = Splitter("##split_list", true, body_h, list_w);
    if (resized >= 0.0f)
        out.Push(app::IntentKind::ResizeList, 0, 0, resized);
    ImGui::SameLine(0.0f, 0.0f);
    ImGui::BeginChild("outing", ImVec2(0.0f, body_h), ImGuiChildFlags_Borders);
    if (model.comparison.open)
        DrawComparisonView(model, view_models, textures, out);
    else if (model.SelectedOuting())
        DrawOutingView(model, view_models, textures, out);
    else
        DrawOverviewView(model, view_models, textures, out);
    ImGui::EndChild();

    DrawStatusBar(model, view_models.map);
    ImGui::End();
    DrawAbout(view_models.main, out);
    DrawTypeConfirmation(model, out);
    DrawErrorPopup(model, out);
}

} // namespace ui
