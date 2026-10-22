// OpenRider -- entry point. Wires the MVC together:
//   model        app::AppModel, changed only by app::AppController
//   view models  owned by their view controllers (main, list, map, outing)
//   views        ui::draw_* -- read-only, report intents
// Each frame: views draw and push intents; then app::step() runs the
// background ticks and routes every intent to its controller.

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <implot.h>
#include <GLFW/glfw3.h>

#include "views.h"

#include "core/log.h"
#include "core/paths.h"
#include "core/plugin_thread.h"
#include "core/plugins.h"

#include <algorithm>
#include <cstdio>

namespace {

std::string ReadFile(const std::string &path)
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

void GlfwError(int code, const char *text)
{
    std::fprintf(stderr, "GLFW error %d: %s\n", code, text);
    core::LogLine("GLFW error %d: %s", code, text);
}

// Plugins found on disk become the controllers' services.
void ConnectServices(app::AppController &ctl, app::MapViewController &map, app::Intents &out)
{
    const std::vector<std::string> roots = {core::BundledPluginsDir(), core::UserPluginsDir()};
    const std::string share = core::BundledShareDir();
    ctl.LoadTypes(ReadFile(share + "/types/labels.json"), "garmin_connect",
                   ReadFile(share + "/types/garmin_connect.json"));
    // GPX files: Garmin Connect's own exports use its type keys; other
    // applications' names on top.
    ctl.LoadTypeSource("gpx", ReadFile(share + "/types/garmin_connect.json"));
    ctl.LoadTypeSource("gpx", ReadFile(share + "/types/gpx.json"));
    ctl.SetOutingTypesFile(core::UserSupportDir() + "/outing_types.json");

    const core::PluginScan filters = core::LoadPlugins(roots, OR_PLUGIN_FILTER);
    for (const core::LoadedPlugin &p : filters.loaded) {
        ctl.AddFilter(static_cast<const or_filter_vtable *>(p.info->vtable));
        core::LogLine("filter plugin %s (%s)", p.info->name, p.path.c_str());
    }
    const core::PluginScan segment_methods = core::LoadPlugins(roots, OR_PLUGIN_SEGMENTS);
    for (const core::LoadedPlugin &p : segment_methods.loaded) {
        ctl.AddSegmentMethod(static_cast<const or_segments_vtable *>(p.info->vtable));
        core::LogLine("segments plugin %s (%s)", p.info->name, p.path.c_str());
    }
    for (const std::string &p : segment_methods.problems)
        out.PushText(app::IntentKind::ReportError, "Plugin refused: " + p);
    const core::PluginScan inputs = core::LoadPlugins(roots, OR_PLUGIN_INPUT);
    for (const std::string &p : filters.problems)
        out.PushText(app::IntentKind::ReportError, "Plugin refused: " + p);
    for (const std::string &p : inputs.problems)
        out.PushText(app::IntentKind::ReportError, "Plugin refused: " + p);
    // Every input source, GarminDB first (it wins over duplicates): how each
    // is opened, its type table, whether its types are to be confirmed.
    struct InputSetup {
        std::string config, types;
        bool confirm_types;
        std::string import_dir;
    };
    auto setup = [](const std::string &name) {
        if (name == "gpx") {
            const std::string dir = core::UserSupportDir() + "/gpx";
            core::MakeDirs(dir);
            return InputSetup{"{\"dir\":\"" + dir + "\"}", "gpx", true, dir};
        }
        return InputSetup{"", name == "garmindb" ? "garmin_connect" : name, false, ""};
    };
    std::vector<core::LoadedPlugin> ordered = inputs.loaded;
    std::stable_sort(ordered.begin(), ordered.end(), [](const core::LoadedPlugin &a, const core::LoadedPlugin &b) {
        return std::string(a.info->name) == "garmindb" && std::string(b.info->name) != "garmindb";
    });
    bool opened = false;
    for (const core::LoadedPlugin &p : ordered) {
        const auto *input = static_cast<const or_input_vtable *>(p.info->vtable);
        const InputSetup how = setup(p.info->name);
        char err[512] = "";
        void *handle = nullptr;
        core::CallPlugin(input, [&] { handle = input->open(how.config.empty() ? nullptr : how.config.c_str(), err, sizeof err); });
        if (!handle) {
            out.PushText(app::IntentKind::ReportError, std::string("Cannot open ") + p.info->name + ": " + err);
            continue;
        }
        core::LogLine("input plugin %s %s (%s)", p.info->name, p.info->version, p.path.c_str());
        ctl.AddInput(input, handle, p.info->name, how.types, how.confirm_types, how.import_dir);
        opened = true;
    }
    if (inputs.loaded.empty())
        out.PushText(app::IntentKind::ReportError, "No input plugin found in " + roots[0] + "/input");
    if (opened)
        ctl.Start();
    // Coordinate systems: those shipped, then the user's own (optional).
    for (const std::string &p : map.LoadCrs(share + "/crs.txt"))
        out.PushText(app::IntentKind::ReportError, "Coordinate systems: " + p);
    const std::string user_crs = core::UserSupportDir() + "/crs.txt";
    if (FILE *file = std::fopen(user_crs.c_str(), "r")) {
        std::fclose(file);
        for (const std::string &p : map.LoadCrs(user_crs))
            out.PushText(app::IntentKind::ReportError, "Coordinate systems: " + p);
    }
    for (const std::string &p : map.LoadLayers({share + "/layers", core::UserSupportDir() + "/layers"}, roots,
                                                core::UserCacheDir() + "/tiles", "opentopomap"))
        out.PushText(app::IntentKind::ReportError, p);
}

} // namespace

// @impl 0f87-6acd-23b0-c555
int main()
{
    core::LogOpenSession(core::UserLogsDir());
    core::LogLine("OpenRider starting");

    glfwSetErrorCallback(GlfwError);
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
    ui::ApplyTheme();
    ui::LoadFonts(core::BundledShareDir() + "/fonts/Phosphor.ttf");
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 150");

    {
        app::AppModel model;
        app::AppController app_ctl(model);
        app::MainViewController main_vc;
        app::ListViewController list_vc;
        app::MapViewController map_vc;
        app::OutingViewController outing_vc;
        app::ComparisonViewController comparison_vc;
        app::Controllers controllers{model, app_ctl, main_vc, list_vc, map_vc, outing_vc, comparison_vc};
        app::Intents intents;
        ConnectServices(app_ctl, map_vc, intents);
        ui::TextureMirror textures;

        while (!glfwWindowShouldClose(window) && !model.quit) {
            glfwPollEvents();
            app::Step(controllers, intents);

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            const ui::ViewModels view_models{main_vc.ViewModel(), list_vc.ViewModel(), map_vc.ViewModel(),
                                             outing_vc.ViewModel(), comparison_vc.ViewModel()};
            ui::DrawMainView(model, view_models, textures, intents);
            ImGui::Render();

            int w, h;
            glfwGetFramebufferSize(window, &w, &h);
            glViewport(0, 0, w, h);
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(window);
        }

        // Background work stops before the services it uses go away.
        app_ctl.Shutdown();
        map_vc.Shutdown();
        app_ctl.CloseInputs();
        core::LogLine("OpenRider exiting");
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
