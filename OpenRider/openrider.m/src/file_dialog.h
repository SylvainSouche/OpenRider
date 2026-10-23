// The system's file browser.

#pragma once

#include <string>
#include <vector>

namespace ui {

/// Asks the user to choose one or several files with the system's file
/// browser (macOS: the standard dialog; Linux: zenity), and waits.
/// @param prompt shown in the dialog
/// @param extension only files with it are returned (e.g. ".gpx"), any case
/// @return the files chosen, none if cancelled or no browser is available
std::vector<std::string> ChooseFiles(const std::string &prompt, const std::string &extension);

} // namespace ui
