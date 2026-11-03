#include "file_dialog.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#endif

namespace ui {

namespace {

#ifdef _WIN32
// The files picked in the standard Windows Open dialog, with '/' separators;
// empty if cancelled. Several may be picked.
std::vector<std::string> OpenDialogFiles(const std::string &prompt, const std::string &extension)
{
    // Filter: "GPX files (*.gpx)\0*.gpx\0\0".
    const std::string pattern = "*" + extension;
    std::string filter = prompt + " (" + pattern + ")";
    filter.push_back('\0');
    filter += pattern;
    filter.push_back('\0');
    filter.push_back('\0');
    std::vector<char> buffer(64 * 1024, '\0');
    OPENFILENAMEA dialog{};
    dialog.lStructSize = sizeof dialog;
    dialog.lpstrFilter = filter.c_str();
    dialog.lpstrFile = buffer.data();
    dialog.nMaxFile = static_cast<DWORD>(buffer.size());
    dialog.lpstrTitle = prompt.c_str();
    dialog.Flags = OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    std::vector<std::string> files;
    if (!GetOpenFileNameA(&dialog))
        return files;
    // One file: its full path. Several: the directory, then each name, every
    // string ended by '\0' and the list by an empty one.
    std::vector<std::string> parts;
    for (const char *at = buffer.data(); *at; at += parts.back().size() + 1)
        parts.emplace_back(at);
    if (parts.size() == 1)
        files.push_back(parts[0]);
    else
        for (size_t i = 1; i < parts.size(); ++i)
            files.push_back(parts[0] + "/" + parts[i]);
    for (std::string &file : files)
        std::replace(file.begin(), file.end(), '\\', '/');
    return files;
}
#else
// The output lines of `command` (run by the shell).
std::vector<std::string> Lines(const std::string &command)
{
    std::vector<std::string> lines;
    FILE *pipe = popen(command.c_str(), "r");
    if (!pipe)
        return lines;
    std::string text;
    char chunk[4096];
    size_t n;
    while ((n = std::fread(chunk, 1, sizeof chunk, pipe)) > 0)
        text.append(chunk, n);
    pclose(pipe);
    for (size_t start = 0, end; start < text.size(); start = end + 1) {
        end = text.find('\n', start);
        if (end == std::string::npos)
            end = text.size();
        if (end > start)
            lines.push_back(text.substr(start, end - start));
    }
    return lines;
}
#endif

bool EndsWith(std::string text, std::string suffix)
{
    auto lower = [](std::string &s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    };
    lower(text);
    lower(suffix);
    return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

} // namespace

std::vector<std::string> ChooseFiles(const std::string &prompt, const std::string &extension)
{
#if defined(_WIN32)
    std::vector<std::string> files;
    for (const std::string &file : OpenDialogFiles(prompt, extension))
        if (EndsWith(file, extension))
            files.push_back(file);
    return files;
#else
#if defined(__APPLE__)
    // AppleScript's standard dialog; a cancel ends the script with an
    // error, so nothing is printed. The prompt carries no quotes.
    const std::string command = "osascript -e 'set chosen to choose file with prompt \"" + prompt +
                                "\" with multiple selections allowed' -e 'set out to \"\"' "
                                "-e 'repeat with f in chosen' -e 'set out to out & POSIX path of f & linefeed' "
                                "-e 'end repeat' -e 'return out' 2>/dev/null";
#elif defined(__linux__)
    const std::string command = "zenity --file-selection --multiple --separator='\\n' --title='" + prompt +
                                "' --file-filter='*" + extension + "' 2>/dev/null";
#else
    const std::string command;
#endif
    std::vector<std::string> files;
    if (command.empty())
        return files;
    for (const std::string &line : Lines(command))
        if (EndsWith(line, extension))
            files.push_back(line);
    return files;
#endif
}

} // namespace ui
