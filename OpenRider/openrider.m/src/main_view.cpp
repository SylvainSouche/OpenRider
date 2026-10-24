// Main view: window, menu bar, the list/panel split, status bar, dialogs.

#include "file_dialog.h"
#include "views.h"

#include "app/credits.h"

#include <algorithm>
#include <string>
#include <vector>

namespace ui {

namespace {

void DrawMenuBar(const app::AppModel &model, app::Intents &out)
{
    if (!ImGui::BeginMenuBar())
        return;
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Import GPX...", nullptr, false, model.can_import && model.importing < 0 && !model.scanning)) {
            // The system's browser; the files are then imported one by one.
            // Without one, say what to install rather than do nothing.
            if (const std::string missing = FileBrowserMissing(); !missing.empty()) {
                out.PushText(app::IntentKind::ReportError, missing);
            } else {
                app::Intent import{app::IntentKind::ImportFiles};
                import.ids = ChooseFiles("Import GPX files", ".gpx");
                if (!import.ids.empty())
                    out.Push(std::move(import));
            }
        }
        ImGui::Separator();
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
    for (const app::Credit &credit : app::kLibraryCredits)
        ImGui::BulletText("%s (%s)%s%s", credit.name, credit.licence, *credit.by ? " -- " : "", credit.by);
    ImGui::TextUnformatted("Data:");
    for (const app::Credit &credit : app::kDataCredits)
        ImGui::BulletText("%s (%s)%s%s", credit.name, credit.licence, *credit.by ? " -- " : "", credit.by);
    if (ImGui::Button("Close")) {
        out.Push(app::IntentKind::ShowAbout, 0);
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

// The categories (sports with a label) and every labelled key.
struct TypeChoices {
    std::vector<std::string> sports;  ///< "running", "cycling"...
    std::vector<std::string> keys;    ///< those and "running/trail"...
};

TypeChoices Choices(const app::AppModel &model)
{
    TypeChoices choices;
    choices.keys = model.types.LabelledKeys();
    for (const std::string &key : choices.keys)
        if (key.find('/') == std::string::npos)
            choices.sports.push_back(key);
    return choices;
}

// Sets outing `i`'s type to `key`.
void SetType(app::Intents &out, size_t i, const std::string &key)
{
    app::Intent intent{app::IntentKind::SetOutingType};
    intent.i = static_cast<int>(i);
    intent.text = key;
    out.Push(std::move(intent));
}

// Outing `i`'s category: one of the sports.
void SportCombo(const app::AppModel &model, size_t i, const TypeChoices &choices, app::Intents &out)
{
    const std::string &sport = model.outings[i].type.sport;
    ImGui::SetNextItemWidth(170.0f);
    if (ImGui::BeginCombo("##sport", model.types.KeyLabel(sport, model.lang).c_str())) {
        for (const std::string &choice : choices.sports)
            if (ImGui::Selectable(model.types.KeyLabel(choice, model.lang).c_str(), choice == sport))
                SetType(out, i, choice + "/generic");
        ImGui::EndCombo();
    }
}

// Outing `i`'s subcategory: none, or one of its sport's.
void SubSportCombo(const app::AppModel &model, size_t i, const TypeChoices &choices, app::Intents &out)
{
    const core::ActivityType &type = model.outings[i].type;
    const std::string current = type.sport + "/" + type.sub_sport;
    ImGui::SetNextItemWidth(170.0f);
    const std::string preview = type.sub_sport == "generic" ? "-" : model.types.KeyLabel(current, model.lang);
    if (ImGui::BeginCombo("##sub", preview.c_str())) {
        if (ImGui::Selectable("-", type.sub_sport == "generic"))
            SetType(out, i, type.sport + "/generic");
        for (const std::string &choice : choices.keys)
            if (choice.rfind(type.sport + "/", 0) == 0 &&
                ImGui::Selectable(model.types.KeyLabel(choice, model.lang).c_str(), choice == current))
                SetType(out, i, choice);
        ImGui::EndCombo();
    }
}

// New outings found in the GPX folder at start whose type is to be
// confirmed: each with its category and subcategory, prefilled from the
// file, changed or confirmed. (Files imported from the menu are asked one
// by one instead.)
void DrawTypeConfirmation(const app::AppModel &model, app::Intents &out)
{
    std::vector<size_t> pending;
    for (size_t i = 0; i < model.outings.size(); ++i)
        if (!model.outings[i].confirmed && static_cast<int>(i) != model.importing)
            pending.push_back(i);
    if (pending.empty() || model.scanning || !model.errors.empty() || model.importing >= 0 ||
        !model.import_queue.empty())
        return;
    const char *title = "New outings: confirm their type";
    if (!ImGui::IsPopupOpen(title))
        ImGui::OpenPopup(title);
    if (!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;
    ImGui::Text("%zu new outing%s from GPX files. Check each type, then confirm.", pending.size(),
                pending.size() == 1 ? "" : "s");
    const TypeChoices choices = Choices(model);
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
            SportCombo(model, i, choices, out);
            ImGui::TableNextColumn();
            SubSportCombo(model, i, choices, out);
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

// The file being imported: what it holds and its type, prefilled from the
// file; OK imports it, Cancel drops it. Then the next file.
void DrawImportPopup(const app::AppModel &model, app::Intents &out)
{
    if (model.importing < 0 || model.importing >= static_cast<int>(model.outings.size()) || !model.errors.empty())
        return;
    const size_t i = static_cast<size_t>(model.importing);
    const core::Outing &outing = model.outings[i];
    const char *title = "Import GPX file";
    if (!ImGui::IsPopupOpen(title))
        ImGui::OpenPopup(title);
    if (!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;
    const std::string file = model.importing_file.substr(model.importing_file.find_last_of('/') + 1);
    ImGui::TextUnformatted(file.c_str());
    if (!model.import_queue.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("(%zu more after this one)", model.import_queue.size());
    }
    ImGui::Separator();
    ImGui::Text("%s", outing.raw.name);
    if (outing.Timed())
        ImGui::TextDisabled("%s  -  %.2f km  -  %.0f min", std::string(outing.raw.start_local).substr(0, 16).c_str(),
                            outing.raw.distance_m / 1000.0, outing.raw.duration_s / 60.0);
    else {
        ImGui::TextDisabled("%s  -  %.2f km", std::string(outing.raw.start_local).substr(0, 16).c_str(),
                            outing.raw.distance_m / 1000.0);
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 420.0f);
        ImGui::TextColored(ImVec4(0.85f, 0.45f, 0.0f, 1.0f), "%s", kNoTimesWarning);
        ImGui::PopTextWrapPos();
    }
    const TypeChoices choices = Choices(model);
    ImGui::PushID(static_cast<int>(i));
    ImGui::TextUnformatted("Category");
    ImGui::SameLine(110.0f);
    SportCombo(model, i, choices, out);
    ImGui::TextUnformatted("Subcategory");
    ImGui::SameLine(110.0f);
    SubSportCombo(model, i, choices, out);
    ImGui::PopID();
    ImGui::Separator();
    if (ImGui::Button("OK", ImVec2(90.0f, 0.0f))) {
        out.Push(app::IntentKind::ImportAccept);
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(90.0f, 0.0f))) {
        out.Push(app::IntentKind::ImportCancel);
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
    DrawMenuBar(model, out);

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
    DrawImportPopup(model, out);
    DrawErrorPopup(model, out);
}

} // namespace ui
