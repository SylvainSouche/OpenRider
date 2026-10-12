#include "core/plugins.h"

#include <dirent.h>
#include <dlfcn.h>

#include <algorithm>

namespace core {

namespace {

bool is_shared_library(const std::string &name)
{
    auto ends_with = [&](const char *s) {
        const std::string suffix(s);
        return name.size() > suffix.size() &&
               name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0;
    };
    return ends_with(".dylib") || ends_with(".so") || name.find(".so.") != std::string::npos ||
           ends_with(".dll");
}

std::vector<std::string> libraries_in(const std::string &dir)
{
    std::vector<std::string> out;
    if (DIR *d = opendir(dir.c_str())) {
        while (dirent *e = readdir(d)) {
            const std::string name = e->d_name;
            if (name[0] != '.' && is_shared_library(name))
                out.push_back(dir + "/" + name);
        }
        closedir(d);
    }
    std::sort(out.begin(), out.end());
    return out;
}

} // namespace

const char *plugin_kind_dir(uint32_t kind)
{
    switch (kind) {
    case OR_PLUGIN_INPUT: return "input";
    case OR_PLUGIN_FILTER: return "filters";
    case OR_PLUGIN_TILES: return "tiles";
    case OR_PLUGIN_WORKFLOW: return "workflows";
    default: return "";
    }
}

std::string plugin_check(const or_plugin_info *info, uint32_t kind)
{
    if (!info)
        return "entry point returned no descriptor";
    if (info->api_version != OR_PLUGIN_API_VERSION)
        return "API version " + std::to_string(info->api_version) + ", expected " +
               std::to_string(OR_PLUGIN_API_VERSION);
    if (info->kind != kind)
        return "is a '" + std::string(plugin_kind_dir(info->kind)) + "' plugin, not '" +
               plugin_kind_dir(kind) + "'";
    if (!info->vtable || !info->name)
        return "incomplete descriptor";
    return "";
}

// @impl 0f87-6acd-24ce-521a
PluginScan load_plugins(const std::vector<std::string> &roots, uint32_t kind)
{
    PluginScan scan;
    for (const std::string &root : roots) {
        for (const std::string &path : libraries_in(root + "/" + plugin_kind_dir(kind))) {
            void *h = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
            if (!h) {
                const char *e = dlerror();
                scan.problems.push_back(path + ": " + (e ? e : "cannot load"));
                continue;
            }
            auto entry = reinterpret_cast<or_plugin_entry_fn>(dlsym(h, OR_PLUGIN_ENTRY_SYMBOL));
            const or_plugin_info *info = entry ? entry() : nullptr;
            const std::string why =
                entry ? plugin_check(info, kind) : "no " OR_PLUGIN_ENTRY_SYMBOL " entry point";
            if (!why.empty()) {
                scan.problems.push_back(path + ": " + why);
                dlclose(h);
                continue;
            }
            scan.loaded.push_back(LoadedPlugin{path, info, h});
        }
    }
    return scan;
}

} // namespace core
