// OpenRider -- GUI entry point. A thin Dear ImGui layer: window, menu bar,
// outing list, outing view and status bar. Logic lives in libcore.

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <implot.h>
#include <openrider/filter.h>
#include <GLFW/glfw3.h>

#include "map.h"

#include "core/library.h"
#include "core/log.h"
#include "core/paths.h"
#include "core/plugins.h"
#include "core/overview.h"
#include "core/series.h"
#include "core/stats.h"
#include "core/types.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {

struct App {
    core::TypeCatalog types;
    std::string lang = "en";

    const or_input_vtable *input = nullptr;
    void *input_handle = nullptr;
    std::string input_name;
    core::Library library;
    std::vector<core::Outing> outings;
    bool scan_reported = false;

    // List state.
    core::OutingFilter filter;
    char from_buf[11] = "";
    char to_buf[11] = "";
    core::SortKey sort_key = core::SortKey::Date;
    bool sort_ascending = false;
    int selected = -1;  // index into outings

    // Selected outing's recorded points: as read, and after the enabled
    // track filters (what the map, graphs and computed statistics use).
    std::vector<or_point> raw_points;
    std::vector<or_point> points;
    std::string points_id;
    core::XAxis xaxis = core::XAxis::Time;
    std::vector<core::SeriesSpec> graphs = {core::SeriesSpec{}};  // starts with one

    MapView map;

    // Overview of all outings (shown when none is selected).
    core::OverviewLoader overview_loader;
    std::vector<core::OverviewTrack> tracks;
    bool overview_started = false;
    bool overview_fitted = false;
    bool showing_overview = true;
    MapViewState overview_state;
    maptiles::GeoBox overview_box{-180, -85, 180, 85};
    bool filter_visible = false;            // list: only tracks within the overview map
    std::set<std::string> visible_ids;

    // Filter plugins, their switches and parameter values (this session).
    struct FilterState {
        const or_filter_vtable *vt = nullptr;
        bool enabled = false;
        std::vector<double> values;
    };
    std::vector<FilterState> filters;
    bool stats_computed = false;   // user's choice when no filter is on
    core::ComputedStats computed;  // from `points`
    int track_style = 0;  // 0 = solid colour, else index into kTrackStyles
    ImVec4 track_colour = ImVec4(0.86f, 0.12f, 0.12f, 0.9f);

    // Layout (pixels), adjusted by dragging the splitters.
    float list_width = 0.0f;  // 0 = not yet set
    float map_height = 0.0f;

    // Graph hover: the sample under the mouse, shared with the map.
    double hover_x = std::numeric_limits<double>::quiet_NaN();
    int hover_index = -1;

    // Status bar and messages.
    std::string status = "Ready";
    float progress = -1.0f;
    std::vector<std::string> errors;  // shown one at a time in a popup
    bool show_about = false;
};

std::string read_file(const std::string &path)
{
    std::string out;
    if (FILE *f = std::fopen(path.c_str(), "rb")) {
        char buf[8192];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof buf, f)) > 0)
            out.append(buf, n);
        std::fclose(f);
    }
    return out;
}

void report_error(App &app, const std::string &msg)
{
    core::log_line("error: %s", msg.c_str());
    app.errors.push_back(msg);
}

void load_types(App &app)
{
    const std::string dir = core::bundled_share_dir() + "/types";
    const std::string labels = read_file(dir + "/labels.json");
    const std::string gc = read_file(dir + "/garmin_connect.json");
    if (labels.empty() || gc.empty()) {
        report_error(app, "Cannot read the activity type tables in " + dir);
        return;
    }
    core::Status s = app.types.load_labels(labels);
    if (!s.ok())
        report_error(app, s.error);
    s = app.types.load_source("garmin_connect", gc);
    if (!s.ok())
        report_error(app, s.error);
}

void open_map(App &app)
{
    for (const std::string &p : map_load_layers(app.map, {core::bundled_share_dir() + "/layers",
                                                          core::user_support_dir() + "/layers"}))
        report_error(app, p);
    if (app.map.layers.empty()) {
        report_error(app, "No map layer found in " + core::bundled_share_dir() + "/layers");
        return;
    }
    int initial = 0;  // OpenTopoMap is the default background
    for (size_t i = 0; i < app.map.layers.size(); ++i)
        if (app.map.layers[i].id == "opentopomap")
            initial = static_cast<int>(i);
    const std::string err = map_select(app.map, initial);
    if (!err.empty())
        report_error(app, "Cannot open the map layer: " + err);
    else
        core::log_line("map layer %s (%zu layers available)", app.map.info->name, app.map.layers.size());
}

void open_filters(App &app)
{
    const core::PluginScan scan = core::load_plugins(
        {core::bundled_plugins_dir(), core::user_plugins_dir()}, OR_PLUGIN_FILTER);
    for (const std::string &p : scan.problems)
        report_error(app, "Plugin refused: " + p);
    for (const core::LoadedPlugin &p : scan.loaded) {
        App::FilterState f;
        f.vt = static_cast<const or_filter_vtable *>(p.info->vtable);
        for (size_t i = 0; i < f.vt->param_count; ++i)
            f.values.push_back(f.vt->params[i].default_value);
        app.filters.push_back(f);
        core::log_line("filter plugin %s (%s)", f.vt->id, p.path.c_str());
    }
}

// Re-applies the enabled track filters to the raw points (non-destructive)
// and recomputes the statistics from the result.
void apply_filters(App &app)
{
    app.points = app.raw_points;
    for (const App::FilterState &f : app.filters) {
        if (!f.enabled || f.vt->kind != OR_FILTER_TRACK || app.points.empty())
            continue;
        std::vector<unsigned char> keep(app.points.size(), 1);
        char err[256] = "";
        if (f.vt->apply_track(app.points.data(), app.points.size(), f.values.data(), keep.data(),
                              err, sizeof err) != 0) {
            report_error(app, std::string("Filter ") + f.vt->label + " failed: " + err);
            continue;
        }
        std::vector<or_point> kept;
        for (size_t i = 0; i < app.points.size(); ++i)
            if (keep[i])
                kept.push_back(app.points[i]);
        core::log_line("filter %s kept %zu of %zu points", f.vt->id, kept.size(), app.points.size());
        app.points.swap(kept);
    }
    app.computed = core::compute_stats(app.points, core::MotionParams{}, &app.raw_points);
    app.hover_index = -1;
}

bool any_filter_on(const App &app)
{
    for (const App::FilterState &f : app.filters)
        if (f.enabled)
            return true;
    return false;
}

// One checkbox per filter plugin, parameters in a popup.
// @impl 0f87-6acd-33a9-64c0
void draw_filter_row(App &app)
{
    if (app.filters.empty())
        return;
    ImGui::TextUnformatted("Filters:");
    bool changed = false;
    for (size_t i = 0; i < app.filters.size(); ++i) {
        App::FilterState &f = app.filters[i];
        ImGui::PushID(static_cast<int>(i));
        ImGui::SameLine();
        changed |= ImGui::Checkbox(f.vt->label, &f.enabled);
        ImGui::SameLine();
        if (ImGui::SmallButton("..."))
            ImGui::OpenPopup("params");
        if (ImGui::BeginPopup("params")) {
            ImGui::TextUnformatted(f.vt->label);
            ImGui::Separator();
            for (size_t k = 0; k < f.vt->param_count; ++k) {
                const or_filter_param &prm = f.vt->params[k];
                ImGui::SetNextItemWidth(160.0f);
                if (ImGui::SliderScalar(prm.label, ImGuiDataType_Double, &f.values[k], &prm.min_value,
                                        &prm.max_value, "%.2f"))
                    changed |= f.enabled;
            }
            if (ImGui::Button("Defaults")) {
                for (size_t k = 0; k < f.vt->param_count; ++k)
                    f.values[k] = f.vt->params[k].default_value;
                changed |= f.enabled;
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
    if (changed)
        apply_filters(app);
}

void open_input(App &app)
{
    const core::PluginScan scan = core::load_plugins(
        {core::bundled_plugins_dir(), core::user_plugins_dir()}, OR_PLUGIN_INPUT);
    for (const std::string &p : scan.problems)
        report_error(app, "Plugin refused: " + p);
    if (scan.loaded.empty()) {
        report_error(app, "No input plugin found in " + core::bundled_plugins_dir() + "/input");
        return;
    }
    const core::LoadedPlugin &plugin = scan.loaded.front();
    core::log_line("input plugin %s %s (%s)", plugin.info->name, plugin.info->version,
                   plugin.path.c_str());
    app.input = static_cast<const or_input_vtable *>(plugin.info->vtable);
    char err[512] = "";
    app.input_handle = app.input->open(nullptr, err, sizeof err);
    if (!app.input_handle) {
        report_error(app, std::string("Cannot open ") + plugin.info->name + ": " + err);
        app.input = nullptr;
        return;
    }
    app.input_name = plugin.info->name;
    app.library.start_scan(app.input, app.input_handle, &app.types, "garmin_connect");
    core::log_line("scanning %s", app.input_name.c_str());
}

// Moves freshly scanned outings in and keeps the status bar current.
void poll_library(App &app)
{
    const std::string err = app.library.take(app.outings);
    if (app.library.scanning()) {
        char buf[128];
        std::snprintf(buf, sizeof buf, "Scanning %s... %zu / %zu", app.input_name.c_str(),
                      app.library.scanned(), app.library.total());
        app.status = buf;
        app.progress = app.library.progress() < 0.0f ? 0.0f : app.library.progress();
        return;
    }
    app.overview_loader.take(app.tracks);
    if (app.overview_loader.loading()) {
        char buf[128];
        std::snprintf(buf, sizeof buf, "Loading tracks for the overview... %zu / %zu",
                      app.overview_loader.done(), app.overview_loader.total());
        app.status = buf;
        app.progress = app.overview_loader.total() == 0
                           ? 0.0f
                           : static_cast<float>(app.overview_loader.done()) /
                                 static_cast<float>(app.overview_loader.total());
        return;
    }
    if (app.overview_started && app.progress >= 0.0f) {
        char buf[128];
        std::snprintf(buf, sizeof buf, "%zu outings, %zu tracks", app.outings.size(), app.tracks.size());
        app.status = buf;
        core::log_line("%s", buf);
    }
    app.progress = -1.0f;
    if (app.input && !app.scan_reported) {
        app.scan_reported = true;
        if (!err.empty())
            report_error(app, "Scanning " + app.input_name + " failed: " + err);
        char buf[128];
        std::snprintf(buf, sizeof buf, "%zu outings from %s", app.outings.size(),
                      app.input_name.c_str());
        app.status = buf;
        core::log_line("%s", buf);
        // Then every track, for the overview map.
        std::vector<std::string> ids;
        for (const core::Outing &o : app.outings)
            ids.push_back(o.raw.id);
        app.overview_loader.start(app.input, app.input_handle, ids);
        app.overview_started = true;
    }
}

void glfw_error(int code, const char *text)
{
    std::fprintf(stderr, "GLFW error %d: %s\n", code, text);
    core::log_line("GLFW error %d: %s", code, text);
}

std::string fmt_duration(double s)
{
    if (std::isnan(s))
        return "-";
    const int total = static_cast<int>(s + 0.5);
    char buf[32];
    std::snprintf(buf, sizeof buf, "%d:%02d:%02d", total / 3600, (total / 60) % 60, total % 60);
    return buf;
}

// Default font plus the Phosphor icon font merged in, so icon glyphs
// (Private Use Area codepoints) can appear in any text.
void load_fonts()
{
    // Explicit sizes on both: ImGui 1.92 merges and offsets glyphs only
    // against an explicit reference size.
    constexpr float kFontSize = 13.0f;
    ImGuiIO &io = ImGui::GetIO();
    ImFontConfig base;
    base.SizePixels = kFontSize;
    io.Fonts->AddFontDefault(&base);
    const std::string path = core::bundled_share_dir() + "/fonts/Phosphor.ttf";
    if (FILE *f = std::fopen(path.c_str(), "rb")) {
        std::fclose(f);
        ImFontConfig cfg;
        cfg.MergeMode = true;
        cfg.GlyphOffset = ImVec2(0.0f, 2.0f);
        static const ImWchar ranges[] = {0xE000, 0xEFFF, 0};
        io.Fonts->AddFontFromFileTTF(path.c_str(), kFontSize, &cfg, ranges);
    } else {
        core::log_line("icon font missing: %s", path.c_str());
    }
}

// Light grey theme: ImGui's light style, with greys instead of white.
// @impl 0f87-6acd-2cff-8135
void apply_theme()
{
    ImGui::StyleColorsLight();
    ImGuiStyle &st = ImGui::GetStyle();
    ImVec4 *c = st.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0.80f, 0.81f, 0.82f, 1.0f);
    c[ImGuiCol_ChildBg] = ImVec4(0.85f, 0.86f, 0.87f, 1.0f);
    c[ImGuiCol_PopupBg] = ImVec4(0.88f, 0.89f, 0.90f, 1.0f);
    c[ImGuiCol_MenuBarBg] = ImVec4(0.74f, 0.75f, 0.77f, 1.0f);
    c[ImGuiCol_FrameBg] = ImVec4(0.92f, 0.92f, 0.93f, 1.0f);
    c[ImGuiCol_TableHeaderBg] = ImVec4(0.74f, 0.75f, 0.77f, 1.0f);
    c[ImGuiCol_TableRowBg] = ImVec4(0.85f, 0.86f, 0.87f, 1.0f);
    c[ImGuiCol_TableRowBgAlt] = ImVec4(0.81f, 0.82f, 0.83f, 1.0f);
    c[ImGuiCol_Border] = ImVec4(0.60f, 0.61f, 0.63f, 1.0f);
    st.WindowRounding = 0.0f;
    st.ChildRounding = 3.0f;
    st.FrameRounding = 3.0f;
}

std::string fmt_number(double v, const char *f)
{
    if (std::isnan(v))
        return "-";
    char buf[32];
    std::snprintf(buf, sizeof buf, f, v);
    return buf;
}

void draw_menu_bar(GLFWwindow *window, App &app)
{
    if (!ImGui::BeginMenuBar())
        return;
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Quit", "Cmd+Q"))
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("About OpenRider"))
            app.show_about = true;
        ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
}

void draw_filters(App &app)
{
    std::set<std::string> keys;
    for (const core::Outing &o : app.outings)
        keys.insert(o.type.key());
    std::string preview = "All types";
    for (const core::Outing &o : app.outings)
        if (o.type.key() == app.filter.type_key) {
            preview = app.types.label(o.type, app.lang);
            break;
        }
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::BeginCombo("##type", preview.c_str())) {
        if (ImGui::Selectable("All types", app.filter.type_key.empty()))
            app.filter.type_key.clear();
        for (const std::string &k : keys) {
            core::ActivityType t;
            for (const core::Outing &o : app.outings)
                if (o.type.key() == k) {
                    t = o.type;
                    break;
                }
            const std::string label = app.types.label(t, app.lang);
            if (ImGui::Selectable((label + "##" + k).c_str(), app.filter.type_key == k))
                app.filter.type_key = k;
        }
        ImGui::EndCombo();
    }
    const float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    ImGui::SetNextItemWidth(half);
    if (ImGui::InputTextWithHint("##from", "from YYYY-MM-DD", app.from_buf, sizeof app.from_buf))
        app.filter.from_date = app.from_buf;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(half);
    if (ImGui::InputTextWithHint("##to", "to YYYY-MM-DD", app.to_buf, sizeof app.to_buf))
        app.filter.to_date = app.to_buf;
}

// Selects outing `i` (-1: none, back to the overview), keeping each mode's
// map view.
void select_outing(App &app, int i)
{
    if (i == app.selected)
        return;
    if (app.selected < 0 && i >= 0)
        app.overview_state = map_get_state(app.map);
    app.selected = i;
    if (i < 0) {
        map_set_state(app.map, app.overview_state);
        app.map.fitted_id.clear();
    }
}

// @impl 0f87-6acd-24ce-ac37
void draw_outing_list(App &app)
{
    draw_filters(app);
    if (ImGui::Checkbox("Filter visible tracks", &app.filter_visible) && app.filter_visible &&
        app.selected >= 0)
        select_outing(app, -1);  // the extent is the overview map's
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Only outings whose track is at least partly within the overview map");

    // Sort order.
    static const char *sort_names[] = {"Date", "Name", "Type", "Distance", "Duration", "Ascent"};
    static const core::SortKey sort_keys[] = {core::SortKey::Date, core::SortKey::Name,
                                              core::SortKey::Type, core::SortKey::Distance,
                                              core::SortKey::Duration, core::SortKey::Ascent};
    int sort = 0;
    for (int k = 0; k < 6; ++k)
        if (sort_keys[k] == app.sort_key)
            sort = k;
    ImGui::TextUnformatted("Sort:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110.0f);
    if (ImGui::Combo("##sort", &sort, sort_names, 6))
        app.sort_key = sort_keys[sort];
    ImGui::SameLine();
    if (ImGui::SmallButton(app.sort_ascending ? "ascending" : "descending"))
        app.sort_ascending = !app.sort_ascending;

    core::OutingFilter filter = app.filter;
    if (app.filter_visible)
        filter.only_ids = &app.visible_ids;
    const std::vector<size_t> rows = core::select_outings(app.outings, filter, app.sort_key, app.sort_ascending);
    ImGui::TextDisabled("%zu outings", rows.size());
    ImGui::Separator();

    // @impl 0f87-6acd-5b42-dc4b
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
            const core::Outing &o = app.outings[i];
            ImGui::PushID(static_cast<int>(i));
            const ImVec2 p = ImGui::GetCursorScreenPos();
            const float w = ImGui::GetContentRegionAvail().x;
            const bool selected = app.selected == static_cast<int>(i);
            if (ImGui::Selectable("##row", selected, ImGuiSelectableFlags_None, ImVec2(0.0f, row_h)))
                select_outing(app, selected ? -1 : static_cast<int>(i));  // click again: overview
            ImDrawList *dl = ImGui::GetWindowDrawList();
            const ImVec4 clip(p.x, p.y, p.x + w, p.y + row_h);
            // Large type icon.
            const std::string icon = app.types.icon(o.type);
            dl->AddText(font, row_h * 0.8f, ImVec2(p.x + 4.0f, p.y + row_h * 0.06f), text, icon.c_str());
            const float x = p.x + row_h + 6.0f;
            // Date, larger, then the name.
            const std::string date = core::long_date(o.raw.start_local, app.lang);
            const float date_size = line * 1.3f;
            dl->AddText(font, date_size, ImVec2(x, p.y + 3.0f), text, date.c_str());
            const float dw = font->CalcTextSizeA(date_size, FLT_MAX, 0.0f, date.c_str()).x;
            dl->AddText(font, line, ImVec2(x + dw + 10.0f, p.y + 3.0f + (date_size - line) * 0.7f), text,
                        o.raw.name, nullptr, 0.0f, &clip);
            // Distance, duration (moving), D+.
            const std::string info = "Distance: " + fmt_number(o.raw.distance_m / 1000.0, "%.1f km") +
                                     "   Duration: " + fmt_duration(o.raw.duration_s) + " (" +
                                     fmt_duration(o.raw.moving_s) + ")   D+ " +
                                     fmt_number(o.raw.ascent_m, "%.0f m");
            dl->AddText(font, line, ImVec2(x, p.y + 6.0f + date_size), dim, info.c_str(), nullptr, 0.0f, &clip);
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
}

void draw_outing_view(App &app)
{
    if (app.selected < 0 || app.selected >= static_cast<int>(app.outings.size())) {
        ImGui::TextDisabled("Select an outing");
        return;
    }
    const core::Outing &o = app.outings[static_cast<size_t>(app.selected)];
    ImGui::Text("%s", o.raw.name);
    ImGui::TextDisabled("%s  -  %s", o.raw.start_local,
                        app.types.label(o.type, app.lang).c_str());
    // Source: Garmin's statistics (Connect summary JSON, else GarminDB) by
    // default; computed from the points when chosen, and always after a
    // track filter.
    const bool filtered = any_filter_on(app);
    if (filtered) {
        ImGui::TextDisabled("Statistics computed from the filtered points");
    } else {
        ImGui::TextUnformatted("Statistics:");
        ImGui::SameLine();
        if (ImGui::RadioButton("Garmin", !app.stats_computed))
            app.stats_computed = false;
        ImGui::SameLine();
        if (ImGui::RadioButton("Computed", app.stats_computed))
            app.stats_computed = true;
    }
    ImGui::Separator();
    or_outing r = o.raw;
    double moving_speed = r.moving_s > 0.0 ? r.distance_m / r.moving_s : std::numeric_limits<double>::quiet_NaN();
    if ((filtered || app.stats_computed) && !app.points.empty()) {
        const core::ComputedStats &c = app.computed;
        r.distance_m = c.distance_m;
        r.duration_s = c.duration_s;
        r.moving_s = c.moving_s;
        r.ascent_m = c.ascent_m;
        r.descent_m = c.descent_m;
        r.min_alt_m = c.min_alt_m;
        r.max_alt_m = c.max_alt_m;
        r.avg_speed_ms = c.avg_speed_ms;
        r.max_speed_ms = c.max_speed_ms;
        moving_speed = c.moving_speed_ms;
    }
    if (ImGui::BeginTable("stats", 4, ImGuiTableFlags_SizingFixedFit)) {
        auto row = [](const char *l1, const std::string &v1, const char *l2, const std::string &v2) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s", l1);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(v1.c_str());
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s", l2);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(v2.c_str());
        };
        row("Distance", fmt_number(r.distance_m / 1000.0, "%.2f km"),
            "Calories", fmt_number(r.calories, "%.0f"));
        row("Duration", fmt_duration(r.duration_s),
            "Moving time", fmt_duration(r.moving_s));
        row("Ascent", fmt_number(r.ascent_m, "%.0f m"),
            "Descent", fmt_number(r.descent_m, "%.0f m"));
        row("Min altitude", fmt_number(r.min_alt_m, "%.0f m"),
            "Max altitude", fmt_number(r.max_alt_m, "%.0f m"));
        row("Average speed", fmt_number(r.avg_speed_ms * 3.6, "%.1f km/h"),
            "Moving speed", fmt_number(moving_speed * 3.6, "%.1f km/h"));
        row("Max speed", fmt_number(r.max_speed_ms * 3.6, "%.1f km/h"), "", "");
        row("Average HR", r.avg_hr < 0 ? "-" : std::to_string(r.avg_hr) + " bpm",
            "Max HR", r.max_hr < 0 ? "-" : std::to_string(r.max_hr) + " bpm");
        row("Temperature", fmt_number(r.avg_temp_c, "%.1f °C"),
            "Min / max", fmt_number(r.min_temp_c, "%.0f") + " / " + fmt_number(r.max_temp_c, "%.0f °C"));
        row("Aerobic TE", fmt_number(r.aerobic_te, "%.1f"),
            "Anaerobic TE", fmt_number(r.anaerobic_te, "%.1f"));
        row("Recorded as", std::string(r.fit_sport) + " / " + r.fit_sub_sport, "", "");
        ImGui::EndTable();
    }
}

// A draggable separator; returns how far it was dragged this frame.
// @impl 0f87-6acd-32ed-1258
float splitter(const char *id, bool vertical, float length)
{
    constexpr float kThickness = 6.0f;
    const ImVec2 size = vertical ? ImVec2(kThickness, length) : ImVec2(length, kThickness);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(id, size);
    const bool active = ImGui::IsItemActive();
    if (ImGui::IsItemHovered() || active)
        ImGui::SetMouseCursor(vertical ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);
    const ImU32 col = ImGui::GetColorU32(active ? ImGuiCol_SeparatorActive
                                                : ImGui::IsItemHovered() ? ImGuiCol_SeparatorHovered
                                                                         : ImGuiCol_Separator);
    ImDrawList *dl = ImGui::GetWindowDrawList();
    if (vertical)
        dl->AddLine(ImVec2(p.x + kThickness / 2, p.y), ImVec2(p.x + kThickness / 2, p.y + length), col, 2.0f);
    else
        dl->AddLine(ImVec2(p.x, p.y + kThickness / 2), ImVec2(p.x + length, p.y + kThickness / 2), col, 2.0f);
    if (!active)
        return 0.0f;
    return vertical ? ImGui::GetIO().MouseDelta.x : ImGui::GetIO().MouseDelta.y;
}

struct TrackStyleOption {
    const char *name;
    core::SeriesKind kind;
};
const TrackStyleOption kTrackStyles[] = {
    {"Solid colour", core::SeriesKind::Altitude},  // kind unused for solid
    {"Speed", core::SeriesKind::Speed},
    {"Heart rate", core::SeriesKind::HeartRate},
    {"Altitude", core::SeriesKind::Altitude},
    {"Grade", core::SeriesKind::Grade},
    {"Vertical speed", core::SeriesKind::VerticalSpeed},
};

// Loads the selected outing's points through the input plugin (once).
void ensure_points(App &app, const core::Outing &o)
{
    if (app.points_id == o.raw.id || !app.input)
        return;
    app.raw_points.clear();
    app.points_id = o.raw.id;
    char err[512] = "";
    auto cb = [](void *user, const or_point *p) -> int {
        static_cast<std::vector<or_point> *>(user)->push_back(*p);
        return 0;
    };
    if (app.input->load_points(app.input_handle, o.raw.id, cb, &app.raw_points, err, sizeof err) != 0)
        report_error(app, std::string("Cannot load points of ") + o.raw.name + ": " + err);
    core::log_line("loaded %zu points for %s", app.raw_points.size(), o.raw.id);
    apply_filters(app);
}

// One row of controls per graph: series, its window/unit, remove.
void draw_graph_controls(App &app)
{
    static const core::SeriesKind kinds[] = {
        core::SeriesKind::Altitude,      core::SeriesKind::Speed,
        core::SeriesKind::HeartRate,     core::SeriesKind::Cadence,
        core::SeriesKind::Temperature,   core::SeriesKind::VerticalSpeed,
        core::SeriesKind::Grade};
    ImGui::TextUnformatted("X axis:");
    ImGui::SameLine();
    if (ImGui::RadioButton("Time", app.xaxis == core::XAxis::Time))
        app.xaxis = core::XAxis::Time;
    ImGui::SameLine();
    if (ImGui::RadioButton("Distance", app.xaxis == core::XAxis::Distance))
        app.xaxis = core::XAxis::Distance;
    ImGui::SameLine();
    if (ImGui::Button("+ Add graph"))
        app.graphs.push_back(core::SeriesSpec{});

    int remove = -1;
    for (size_t g = 0; g < app.graphs.size(); ++g) {
        core::SeriesSpec &s = app.graphs[g];
        ImGui::PushID(static_cast<int>(g));
        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::BeginCombo("##series", core::series_label(s.kind))) {
            for (core::SeriesKind k : kinds)
                if (ImGui::Selectable(core::series_label(k), s.kind == k))
                    s.kind = k;
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(95.0f);
        int type = s.type == core::GraphType::Histogram ? 1 : 0;
        static const char *type_names[] = {"Curve", "Histogram"};
        if (ImGui::Combo("##type", &type, type_names, 2))
            s.type = type == 1 ? core::GraphType::Histogram : core::GraphType::Curve;
        if (s.kind == core::SeriesKind::VerticalSpeed) {
            static const double windows[] = {30.0, 60.0, 300.0};
            static const char *window_names[] = {"30 s", "1 min", "5 min"};
            static const core::VSpeedUnit units[] = {core::VSpeedUnit::MetresPerHour,
                                                     core::VSpeedUnit::MetresPerMinute,
                                                     core::VSpeedUnit::MetresPerSecond};
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80.0f);
            int w = s.vspeed_window_s == 30.0 ? 0 : s.vspeed_window_s == 300.0 ? 2 : 1;
            if (ImGui::Combo("##window", &w, window_names, 3))
                s.vspeed_window_s = windows[w];
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80.0f);
            if (ImGui::BeginCombo("##unit", core::series_unit(s))) {
                for (core::VSpeedUnit u : units) {
                    core::SeriesSpec t = s;
                    t.vspeed_unit = u;
                    if (ImGui::Selectable(core::series_unit(t), s.vspeed_unit == u))
                        s.vspeed_unit = u;
                }
                ImGui::EndCombo();
            }
        } else if (s.kind == core::SeriesKind::Grade) {
            static const char *window_names[] = {"100 m", "1000 m"};
            int w = s.grade_window_m == 1000.0 ? 1 : 0;
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80.0f);
            if (ImGui::Combo("##gwindow", &w, window_names, 2))
                s.grade_window_m = w == 1 ? 1000.0 : 100.0;
        }
        if (app.graphs.size() > 1) {
            ImGui::SameLine();
            if (ImGui::SmallButton("Remove"))
                remove = static_cast<int>(g);
        }
        ImGui::PopID();
    }
    if (remove >= 0)
        app.graphs.erase(app.graphs.begin() + remove);
}

// @impl 0f87-6acd-2d65-0c79
void draw_graphs(App &app, float height)
{
    if (app.points.empty()) {
        ImGui::TextDisabled("No recorded points");
        return;
    }
    const int rows = static_cast<int>(app.graphs.size());
    const ImPlotSubplotFlags flags = ImPlotSubplotFlags_LinkAllX | ImPlotSubplotFlags_NoTitle;
    if (!ImPlot::BeginSubplots("##graphs", rows, 1, ImVec2(-1.0f, height), flags))
        return;
    const char *xlabel = app.xaxis == core::XAxis::Time ? "time (min)" : "distance (km)";
    int hovered = -1;
    double hovered_x = std::numeric_limits<double>::quiet_NaN();
    for (int g = 0; g < rows; ++g) {
        const core::SeriesSpec &s = app.graphs[static_cast<size_t>(g)];
        const bool histogram = s.type == core::GraphType::Histogram;
        const core::Series series = histogram ? core::histogram_series(app.points, app.xaxis, s)
                                              : core::compute_series(app.points, app.xaxis, s);
        char ylabel[64];
        std::snprintf(ylabel, sizeof ylabel, "%s (%s)", core::series_label(s.kind),
                      core::series_unit(s));
        char id[16];
        std::snprintf(id, sizeof id, "##g%d", g);
        if (ImPlot::BeginPlot(id, ImVec2(-1.0f, 0.0f), ImPlotFlags_NoLegend)) {
            ImPlot::SetupAxes(g == rows - 1 ? xlabel : nullptr, ylabel,
                              ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
            if (histogram) {
                ImPlotSpec bars;
                bars.Flags = ImPlotStairsFlags_Shaded;
                bars.FillAlpha = 0.5f;
                ImPlot::PlotStairs(ylabel, series.x.data(), series.y.data(),
                                   static_cast<int>(series.x.size()), bars);
            } else {
                ImPlot::PlotLine(ylabel, series.x.data(), series.y.data(),
                                 static_cast<int>(series.x.size()));
            }
            if (!std::isnan(app.hover_x)) {
                ImPlotSpec cursor;
                cursor.LineColor = ImVec4(0.2f, 0.2f, 0.2f, 0.6f);
                ImPlot::PlotInfLines("##cursor", &app.hover_x, 1, cursor);
            }
            // @impl 0f87-6acd-32ed-0184
            if (ImPlot::IsPlotHovered()) {
                const double mx = ImPlot::GetPlotMousePos().x;
                size_t best = 0;
                if (histogram) {
                    // The bar under the mouse: the last edge at or left of it.
                    for (size_t i = 0; i + 1 < series.x.size(); ++i)
                        if (series.x[i] <= mx)
                            best = i;
                } else {
                    double best_d = std::numeric_limits<double>::infinity();
                    for (size_t i = 0; i < series.x.size(); ++i) {
                        const double d = std::fabs(series.x[i] - mx);
                        if (d < best_d) {
                            best_d = d;
                            best = i;
                        }
                    }
                }
                hovered = static_cast<int>(best < series.point_index.size() ? series.point_index[best] : best);
                hovered_x = series.x[best];
                ImGui::BeginTooltip();
                ImGui::Text("%s  %.2f", xlabel, series.x[best]);
                if (std::isnan(series.y[best]))
                    ImGui::Text("%s  -", ylabel);
                else
                    ImGui::Text("%s  %.1f", ylabel, series.y[best]);
                ImGui::EndTooltip();
            }
            ImPlot::EndPlot();
        }
    }
    ImPlot::EndSubplots();
    app.hover_index = hovered;
    app.hover_x = hovered_x;
}

// Background layer choice: the layers covering the view (and the track).
void draw_background_combo(App &app, const std::vector<or_point> &points, float w, float h)
{
    ImGui::TextUnformatted("Background:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200.0f);
    const char *current_name =
        app.map.current >= 0 ? app.map.layers[static_cast<size_t>(app.map.current)].name.c_str() : "-";
    if (ImGui::BeginCombo("##background", current_name)) {
        for (int i : map_offered_layers(app.map, points, w, h))
            if (ImGui::Selectable(app.map.layers[static_cast<size_t>(i)].name.c_str(), i == app.map.current)) {
                const std::string err = map_select(app.map, i);
                if (!err.empty())
                    report_error(app, "Cannot open the map layer: " + err);
                else
                    core::log_line("map layer %s", app.map.info->name);
            }
        ImGui::EndCombo();
    }
}

// No outing selected: every track on the map; click one to open it.
void draw_overview(App &app)
{
    ImGui::Text("All outings  -  %zu tracks loaded", app.tracks.size());
    ImGui::SameLine();
    const ImVec2 avail0 = ImGui::GetContentRegionAvail();
    draw_background_combo(app, {}, avail0.x, avail0.y);
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    // Only the outings passing the list's type and date filters, each in its
    // activity's colour.
    // @impl 0f87-6acd-6195-c32d
    std::map<std::string, size_t> listed;
    for (size_t i : core::select_outings(app.outings, app.filter, app.sort_key, app.sort_ascending))
        listed[app.outings[i].raw.id] = i;
    std::vector<const core::OverviewTrack *> shown;
    std::vector<unsigned int> colours;
    for (const core::OverviewTrack &t : app.tracks) {
        const auto it = listed.find(t.id);
        if (it == listed.end())
            continue;
        const unsigned int rgb = app.types.colour(app.outings[it->second].type);
        shown.push_back(&t);
        colours.push_back(IM_COL32((rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff, 200));
    }
    const bool fit = !app.overview_fitted && !app.overview_loader.loading() && !app.tracks.empty();
    std::string hovered, msg;
    const std::string clicked = map_draw_overview(app.map, shown, colours, avail.x, avail.y, fit, hovered, msg);
    if (fit)
        app.overview_fitted = true;
    if (!msg.empty())
        report_error(app, msg);
    // The extent the list can be restricted to.
    app.overview_box = map_view_box(app.map, avail.x, avail.y);
    app.visible_ids.clear();
    for (const core::OverviewTrack &t : app.tracks)
        if (core::track_in_box(t, app.overview_box.west, app.overview_box.south, app.overview_box.east,
                               app.overview_box.north))
            app.visible_ids.insert(t.id);
    if (!hovered.empty() || !clicked.empty()) {
        for (size_t i = 0; i < app.outings.size(); ++i) {
            const core::Outing &o = app.outings[i];
            if (!hovered.empty() && hovered == o.raw.id)
                ImGui::SetTooltip("%s\n%s", core::long_date(o.raw.start_local, app.lang).c_str(), o.raw.name);
            if (!clicked.empty() && clicked == o.raw.id)
                select_outing(app, static_cast<int>(i));
        }
    }
}

void draw_outing_panel(App &app)
{
    if (app.selected < 0 || app.selected >= static_cast<int>(app.outings.size())) {
        draw_overview(app);
        return;
    }
    if (ImGui::SmallButton("< All outings"))
        select_outing(app, -1);
    ImGui::SameLine();
    ensure_points(app, app.outings[static_cast<size_t>(app.selected)]);
    if (ImGui::CollapsingHeader("Statistics", ImGuiTreeNodeFlags_DefaultOpen))
        draw_outing_view(app);
    else
        ImGui::Text("%s", app.outings[static_cast<size_t>(app.selected)].raw.name);
    ImGui::Separator();
    draw_filter_row(app);

    // Background layer: those covering the view and the track.
    const ImVec2 avail_map = ImGui::GetContentRegionAvail();
    draw_background_combo(app, app.points, avail_map.x,
                          app.map_height > 0.0f ? app.map_height : avail_map.y * 0.45f);
    ImGui::SameLine();

    // Track style: solid colour or coloured by a series.
    ImGui::TextUnformatted("Track:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150.0f);
    if (ImGui::BeginCombo("##trackstyle", kTrackStyles[app.track_style].name)) {
        for (int i = 0; i < static_cast<int>(IM_ARRAYSIZE(kTrackStyles)); ++i)
            if (ImGui::Selectable(kTrackStyles[i].name, app.track_style == i))
                app.track_style = i;
        ImGui::EndCombo();
    }
    if (app.track_style == 0) {
        ImGui::SameLine();
        ImGui::ColorEdit4("##trackcolour", &app.track_colour.x,
                          ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
    }
    TrackPaint paint;
    paint.solid = ImGui::ColorConvertFloat4ToU32(app.track_colour);
    paint.hover_index = app.hover_index;
    std::vector<double> values;
    char legend[64] = "";
    if (app.track_style > 0) {
        core::SeriesSpec spec;
        spec.kind = kTrackStyles[app.track_style].kind;
        values = core::compute_series(app.points, app.xaxis, spec).y;
        std::snprintf(legend, sizeof legend, "%s (%s)", core::series_label(spec.kind),
                      core::series_unit(spec));
        paint.values = &values;
        paint.label = legend;
    }

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    if (app.map_height <= 0.0f)
        app.map_height = avail.y * 0.45f;
    app.map_height = std::clamp(app.map_height, 120.0f, std::max(120.0f, avail.y - 160.0f));
    const std::string map_msg = map_draw(app.map, app.points, app.points_id, avail.x, app.map_height, paint);
    if (!map_msg.empty())
        report_error(app, map_msg);
    app.map_height += splitter("##split_map", false, avail.x);
    draw_graph_controls(app);
    draw_graphs(app, ImGui::GetContentRegionAvail().y);
}

// @impl 0f87-6acd-23b0-7c7d
void draw_status_bar(const App &app)
{
    ImGui::Separator();
    std::string status = app.status;
    if (app.map.fetcher && app.map.fetcher->errors() > 0)
        status += "  |  map: " + std::to_string(app.map.fetcher->errors()) + " tile errors";
    if (app.map.fetcher && app.map.fetcher->pending() > 0)
        status += "  |  loading " + std::to_string(app.map.fetcher->pending()) + " tiles";
    ImGui::TextUnformatted(status.c_str());
    if (app.progress >= 0.0f) {
        ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.6f);
        ImGui::ProgressBar(app.progress, ImVec2(-1.0f, 0.0f));
    }
}

void draw_about(App &app)
{
    if (app.show_about) {
        ImGui::OpenPopup("About OpenRider");
        app.show_about = false;
    }
    if (ImGui::BeginPopupModal("About OpenRider", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("OpenRider -- manage and display outings");
        ImGui::TextUnformatted("BSD-3-Clause, (c) 2026 Sylvain Souche");
        ImGui::Separator();
        ImGui::TextUnformatted("Credits:");
        ImGui::BulletText("Dear ImGui (MIT) -- Omar Cornut and contributors");
        ImGui::BulletText("GLFW (zlib) -- Marcus Geelnard, Camilla Loewy");
        ImGui::BulletText("ImPlot (MIT) -- Evan Pezent");
        ImGui::BulletText("cJSON (MIT) -- Dave Gamble and contributors");
        ImGui::BulletText("SQLite (public domain)");
        ImGui::BulletText("stb_image (public domain / MIT) -- Sean Barrett");
        ImGui::BulletText("libcurl (curl licence) -- Daniel Stenberg and contributors");
        ImGui::BulletText("libtiff (libtiff licence) -- Sam Leffler, Silicon Graphics");
        ImGui::BulletText("Phosphor icons (MIT) -- Phosphor Icons");
        ImGui::BulletText("Data: GarminDB output (read only)");
        ImGui::BulletText("Map: OpenTopoMap (CC-BY-SA), map data OpenStreetMap contributors, SRTM");
        if (ImGui::Button("Close"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void draw_error_popup(App &app)
{
    if (app.errors.empty())
        return;
    if (!ImGui::IsPopupOpen("Error"))
        ImGui::OpenPopup("Error");
    if (ImGui::BeginPopupModal("Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PushTextWrapPos(500.0f);
        ImGui::TextUnformatted(app.errors.front().c_str());
        ImGui::PopTextWrapPos();
        if (ImGui::Button("OK")) {
            app.errors.erase(app.errors.begin());
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void draw_main(GLFWwindow *window, App &app)
{
    const ImGuiViewport *vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus |
                                   ImGuiWindowFlags_MenuBar;
    ImGui::Begin("OpenRider", nullptr, flags);
    draw_menu_bar(window, app);

    const float status_h = ImGui::GetFrameHeightWithSpacing() + 4.0f;
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float body_h = avail.y - status_h;

    if (app.list_width <= 0.0f)
        app.list_width = std::min(420.0f, avail.x * 0.32f);
    app.list_width = std::clamp(app.list_width, 200.0f, std::max(200.0f, avail.x - 300.0f));
    ImGui::BeginChild("outings", ImVec2(app.list_width, body_h), ImGuiChildFlags_Borders);
    draw_outing_list(app);
    ImGui::EndChild();

    ImGui::SameLine(0.0f, 0.0f);
    app.list_width += splitter("##split_list", true, body_h);
    ImGui::SameLine(0.0f, 0.0f);
    ImGui::BeginChild("outing", ImVec2(0.0f, body_h), ImGuiChildFlags_Borders);
    draw_outing_panel(app);
    ImGui::EndChild();

    draw_status_bar(app);
    ImGui::End();
    draw_about(app);
    draw_error_popup(app);
}

} // namespace

// @impl 0f87-6acd-23b0-c555
int main()
{
    core::log_open_session(core::user_logs_dir());
    core::log_line("OpenRider starting");

    glfwSetErrorCallback(glfw_error);
    if (!glfwInit())
        return 1;

    // OpenGL 3.2 core: the highest common denominator macOS still offers.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

    GLFWwindow *window = glfwCreateWindow(1280, 800, "OpenRider", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    apply_theme();
    load_fonts();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 150");

    App app;
    load_types(app);
    open_filters(app);
    open_input(app);
    open_map(app);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        poll_library(app);
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        draw_main(window, app);

        ImGui::Render();
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    // Wait for a scan still running before closing the input it reads from.
    while (app.library.scanning())
        glfwWaitEventsTimeout(0.01);
    app.overview_loader.stop();
    if (app.input && app.input_handle)
        app.input->close(app.input_handle);
    map_close(app.map);
    core::log_line("OpenRider exiting");

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
