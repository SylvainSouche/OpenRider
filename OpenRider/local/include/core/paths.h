/// @file
/// Locations the application reads from and writes to (module-level
/// utilities). macOS and Windows; other platforms use the macOS layout for now.
#pragma once

#include <string>

namespace core {

/// Directory holding the running binary.
std::string ExecutableDir();

/// Plugins shipped with the application: `<exe>/../plugins`
/// (Contents/PlugIns in a bundle, later).
std::string BundledPluginsDir();

/// Resources shipped with the application: `<exe>/../share/openrider`.
std::string BundledShareDir();

/// The user's application data: `~/Library/Application Support/OpenRider`
/// (Windows: `%APPDATA%/OpenRider`, roaming with the profile).
std::string UserSupportDir();

/// The user's own plugins: `<UserSupportDir()>/plugins`.
std::string UserPluginsDir();

/// Session logs: `~/Library/Logs/OpenRider` (Windows: `%LOCALAPPDATA%/OpenRider/Logs`).
std::string UserLogsDir();

/// Caches (map tiles...): `~/Library/Caches/OpenRider`
/// (Windows: `%LOCALAPPDATA%/OpenRider/Cache`).
std::string UserCacheDir();

/// Creates `path` and its parents (mkdir -p).
/// @return the directory exists afterwards
bool MakeDirs(const std::string &path);

} // namespace core
