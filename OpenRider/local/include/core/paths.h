// Locations the application reads from and writes to. macOS for now; the
// cross-platform layout is decided later.
#pragma once

#include <string>

namespace core {

std::string executable_dir();          // directory holding the running binary
std::string bundled_plugins_dir();     // <exe>/../plugins (Contents/PlugIns in a bundle, later)
std::string bundled_share_dir();       // <exe>/../share/openrider
std::string user_support_dir();        // ~/Library/Application Support/OpenRider
std::string user_plugins_dir();        // <user_support_dir>/plugins
std::string user_logs_dir();           // ~/Library/Logs/OpenRider
std::string user_cache_dir();          // ~/Library/Caches/OpenRider

bool make_dirs(const std::string &path);  // mkdir -p; true if it exists afterwards

} // namespace core
