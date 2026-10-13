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
#include "core/plugins.h"

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
void ConnectServices(app::AppController &ctl, app::MapViewController &map, app::Intents &out, void *&input_handle,
                      const or_input_vtable *&input)
{
    const std::vector<std::string> roots = {core::BundledPluginsDir(), core::UserPluginsDir()};
    const std::string share = core::BundledShareDir();
    ctl.LoadTypes(ReadFile(share + "/types/labels.json"), "garmin_connect",
                   ReadFile(share + "/types/garmin_connect.json"));

    const core::PluginScan filters = core::LoadPlugins(roots, OR_PLUGIN_FILTER);
    for (const core::LoadedPlugin &p : filters.loaded) {
        ctl.AddFilter(static_cast<const or_filter_vtable *>(p.info->vtable));
        core::LogLine("filter plugin %s (%s)", p.info->name, p.path.c_str());
    }
    const core::PluginScan inputs = core::LoadPlugins(roots, OR_PLUGIN_INPUT);
    for (const std::string &p : filters.problems)
        out.PushText(app::IntentKind::ReportError, "Plugin refused: " + p);
    for (const std::string &p : inputs.problems)
        out.PushText(app::IntentKind::ReportError, "Plugin refused: " + p);
    if (inputs.loaded.empty()) {
        out.PushText(app::IntentKind::ReportError, "No input plugin found in " + roots[0] + "/input");
    } else {
        const core::LoadedPlugin &p = inputs.loaded.front();
        input = static_cast<const or_input_vtable *>(p.info->vtable);
        char err[512] = "";
        input_handle = input->open(nullptr, err, sizeof err);
        if (!input_handle) {
            out.PushText(app::IntentKind::ReportError, std::string("Cannot open ") + p.info->name + ": " + err);
            input = nullptr;
        } else {
            core::LogLine("input plugin %s %s (%s)", p.info->name, p.info->version, p.path.c_str());
            ctl.SetInput(input, input_handle, p.info->name);
            ctl.Start();
        }
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
        app::Controllers controllers{model, app_ctl, main_vc, list_vc, map_vc, outing_vc};
        app::Intents intents;
        const or_input_vtable *input = nullptr;
        void *input_handle = nullptr;
        ConnectServices(app_ctl, map_vc, intents, input_handle, input);
        ui::TextureMirror textures;

        while (!glfwWindowShouldClose(window) && !model.quit) {
            glfwPollEvents();
            app::Step(controllers, intents);

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            const ui::ViewModels view_models{main_vc.ViewModel(), list_vc.ViewModel(), map_vc.ViewModel(), outing_vc.ViewModel()};
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
        if (input && input_handle)
            input->close(input_handle);
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
