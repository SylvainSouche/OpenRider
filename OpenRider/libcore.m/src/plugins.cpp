#include "core/plugins.h"

#include "core/plugin_thread.h"

#include <openrider/filter.h>
#include <openrider/input.h>
#include <openrider/segments.h>
#include <openrider/tiles.h>

#include <dirent.h>
#include <dlfcn.h>

#include <algorithm>
#include <cstddef>

namespace core {

namespace {

bool IsSharedLibrary(const std::string &name)
{
    auto ends_with = [&](const char *s) {
        const std::string suffix(s);
        return name.size() > suffix.size() &&
               name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0;
    };
    return ends_with(".dylib") || ends_with(".so") || name.find(".so.") != std::string::npos ||
           ends_with(".dll");
}

std::vector<std::string> LibrariesIn(const std::string &dir)
{
    std::vector<std::string> out;
    if (DIR *d = opendir(dir.c_str())) {
        while (dirent *e = readdir(d)) {
            const std::string name = e->d_name;
            if (name[0] != '.' && IsSharedLibrary(name))
                out.push_back(dir + "/" + name);
        }
        closedir(d);
    }
    std::sort(out.begin(), out.end());
    return out;
}

} // namespace

size_t VtableMinimumSize(uint32_t kind)
{
    // The tables as of ABI 2.0: later minor versions append to them, these
    // sizes stay.
    switch (kind) {
    case OR_PLUGIN_INPUT: return offsetof(or_input_vtable, load_points) + sizeof(or_input_vtable::load_points);
    case OR_PLUGIN_FILTER: return offsetof(or_filter_vtable, apply_track) + sizeof(or_filter_vtable::apply_track);
    case OR_PLUGIN_TILES: return offsetof(or_tiles_vtable, tile_url) + sizeof(or_tiles_vtable::tile_url);
    case OR_PLUGIN_SEGMENTS:
        return offsetof(or_segments_vtable, free_network) + sizeof(or_segments_vtable::free_network);
    default: return sizeof(size_t);
    }
}

const char *PluginKindDir(uint32_t kind)
{
    switch (kind) {
    case OR_PLUGIN_INPUT: return "input";
    case OR_PLUGIN_FILTER: return "filters";
    case OR_PLUGIN_TILES: return "tiles";
    case OR_PLUGIN_WORKFLOW: return "workflows";
    case OR_PLUGIN_SEGMENTS: return "segments";
    default: return "";
    }
}

std::string PluginCheck(const or_plugin_info *info, uint32_t kind)
{
    if (!info)
        return "entry point returned no descriptor";
    if (info->api_version != OR_PLUGIN_API_VERSION)
        return "API version " + std::to_string(info->api_version) + ", expected " +
               std::to_string(OR_PLUGIN_API_VERSION);
    if (info->api_minor > OR_PLUGIN_API_MINOR)
        return "API version " + std::to_string(info->api_version) + "." + std::to_string(info->api_minor) +
               ", newer than " + std::to_string(OR_PLUGIN_API_VERSION) + "." + std::to_string(OR_PLUGIN_API_MINOR);
    if (info->kind != kind)
        return "is a '" + std::string(PluginKindDir(info->kind)) + "' plugin, not '" +
               PluginKindDir(kind) + "'";
    if (!info->vtable || !info->name)
        return "incomplete descriptor";
    const size_t size = *static_cast<const size_t *>(info->vtable);
    if (size < VtableMinimumSize(kind))
        return "function table of " + std::to_string(size) + " bytes, expected " +
               std::to_string(VtableMinimumSize(kind)) + " at least";
    return "";
}

// @impl 0f87-6acd-24ce-521a
PluginScan LoadPlugins(const std::vector<std::string> &roots, uint32_t kind)
{
    PluginScan scan;
    for (const std::string &root : roots) {
        for (const std::string &path : LibrariesIn(root + "/" + PluginKindDir(kind))) {
            void *h = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
            if (!h) {
                const char *e = dlerror();
                scan.problems.push_back(path + ": " + (e ? e : "cannot load"));
                continue;
            }
            auto entry = reinterpret_cast<or_plugin_entry_fn>(dlsym(h, OR_PLUGIN_ENTRY_SYMBOL));
            const or_plugin_info *info = entry ? entry() : nullptr;
            const std::string why =
                entry ? PluginCheck(info, kind) : "no " OR_PLUGIN_ENTRY_SYMBOL " entry point";
            if (!why.empty()) {
                scan.problems.push_back(path + ": " + why);
                dlclose(h);
                continue;
            }
            if (!(info->flags & OR_PLUGIN_THREAD_SAFE))
                SerialisePlugin(info->vtable);
            scan.loaded.push_back(LoadedPlugin{path, info, h});
        }
    }
    return scan;
}

} // namespace core
