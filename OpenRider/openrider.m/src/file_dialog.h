// The system's file browser.

#pragma once

#include <string>
#include <vector>

namespace ui {

/// Asks the user to choose one or several files with the system's file
/// browser (macOS and Windows: the standard dialog; Linux and the BSDs:
/// zenity, else kdialog), and waits.
/// @param prompt shown in the dialog
/// @param extension only files with it are returned (e.g. ".gpx"), any case
/// @return the files chosen, none if cancelled or no browser is available
std::vector<std::string> ChooseFiles(const std::string &prompt, const std::string &extension);

/// Why no file browser can be shown ("" when one can): on Linux and the
/// BSDs, neither zenity nor kdialog is installed.
std::string FileBrowserMissing();

} // namespace ui
