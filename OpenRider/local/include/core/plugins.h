// Plugin discovery and loading. Plugins are shared libraries under
// <dir>/<kind>/, searched first in the bundled plugins directory, then in
// the user's. Each exports OR_PLUGIN_ENTRY_SYMBOL; a plugin whose API
// version differs from the application's is refused.
#pragma once

#include <openrider/plugin.h>

#include <string>
#include <vector>

namespace core {

struct LoadedPlugin {
    std::string path;
    const or_plugin_info *info = nullptr;
    void *handle = nullptr;  // dlopen handle; kept open for the process lifetime
};

struct PluginScan {
    std::vector<LoadedPlugin> loaded;
    std::vector<std::string> problems;  // one message per refused/broken library
};

const char *plugin_kind_dir(uint32_t kind);  // "input", "filters", ...

// Why an info block is unacceptable for `kind`, or "" if it is fine.
std::string plugin_check(const or_plugin_info *info, uint32_t kind);

// Loads every plugin of `kind` found under each of `roots`/<kind>/.
PluginScan load_plugins(const std::vector<std::string> &roots, uint32_t kind);

} // namespace core
