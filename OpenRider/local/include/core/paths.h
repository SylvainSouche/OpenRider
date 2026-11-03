/// @file
/// Locations the application reads from and writes to (module-level
/// utilities): macOS's Library folders, Windows' profile folders, and the
/// XDG base directories on Linux and the BSDs.
#pragma once

#include <string>

namespace core {

/// Directory holding the running binary (macOS: dyld; Windows:
/// GetModuleFileName; FreeBSD and NetBSD: sysctl; Linux: /proc/self/exe);
/// "." if it cannot be found.
std::string ExecutableDir();

/// Plugins shipped with the application: `<exe>/../plugins`
/// (Contents/PlugIns in a bundle, later).
std::string BundledPluginsDir();

/// Resources shipped with the application: `<exe>/../share/openrider`.
std::string BundledShareDir();

/// The user's application data: `~/Library/Application Support/OpenRider`
/// (Windows: `%APPDATA%/OpenRider`, roaming with the profile; Linux and the
/// BSDs: `$XDG_DATA_HOME/OpenRider`, by default `~/.local/share/OpenRider`).
std::string UserSupportDir();

/// The user's own plugins: `<UserSupportDir()>/plugins`.
std::string UserPluginsDir();

/// Session logs: `~/Library/Logs/OpenRider` (Windows: `%LOCALAPPDATA%/OpenRider/Logs`;
/// Linux and the BSDs: `$XDG_STATE_HOME/OpenRider/logs`, by default
/// `~/.local/state/OpenRider/logs`).
std::string UserLogsDir();

/// Caches (map tiles...): `~/Library/Caches/OpenRider`
/// (Windows: `%LOCALAPPDATA%/OpenRider/Cache`; Linux and the BSDs:
/// `$XDG_CACHE_HOME/OpenRider`, by default `~/.cache/OpenRider`).
std::string UserCacheDir();

/// Creates `path` and its parents (mkdir -p).
/// @return the directory exists afterwards
bool MakeDirs(const std::string &path);

} // namespace core
