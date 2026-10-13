/// @file
/// Plugin discovery and loading (module-level utilities). Plugins are
/// shared libraries under `<dir>/<kind>/`, searched first in the bundled
/// plugins directory, then in the user's. Each exports
/// OR_PLUGIN_ENTRY_SYMBOL; a plugin whose API version differs from the
/// application's is refused.
#pragma once

#include <openrider/plugin.h>

#include <string>
#include <vector>

namespace core {

/// A plugin library loaded and accepted.
struct LoadedPlugin {
    std::string path;                      ///< library file
    const or_plugin_info *info = nullptr;  ///< its descriptor
    void *handle = nullptr;                ///< dlopen handle; kept open for the process lifetime
};

/// The outcome of loading the plugins of one kind.
struct PluginScan {
    std::vector<LoadedPlugin> loaded;   ///< accepted plugins
    std::vector<std::string> problems;  ///< one message per refused/broken library
};

/// Subdirectory of a plugin kind: "input", "filters", "tiles", "workflows".
const char *PluginKindDir(uint32_t kind);

/// Size of a `kind`'s function table as of ABI 2.0: a plugin's table
/// (its struct_size) is at least this large.
size_t VtableMinimumSize(uint32_t kind);

/// Why a descriptor is unacceptable for `kind`, or "" if it is fine: another
/// major ABI version, a newer minor one, another kind, a missing field, or
/// a function table smaller than VtableMinimumSize().
std::string PluginCheck(const or_plugin_info *info, uint32_t kind);

/// Loads every plugin of `kind` found under `<root>/<kind dir>/` for each of `roots`.
PluginScan LoadPlugins(const std::vector<std::string> &roots, uint32_t kind);

} // namespace core
