#include "file_dialog.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

#if defined(_WIN32)
#include <windows.h>
#include <commdlg.h>
#include <cwchar>
#endif

namespace ui {

namespace {

#if !defined(_WIN32)
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

// @impl 0f87-6adb-701e-a77c
#if defined(_WIN32)
// The Windows open-file dialog, several files allowed: the folder, then
// each file's name, separated by NULs (a single file: its full path).
std::vector<std::string> ChooseFiles(const std::string &prompt, const std::string &extension)
{
    auto wide = [](const std::string &text) {
        std::wstring out(static_cast<size_t>(MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0)), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, out.data(), static_cast<int>(out.size()));
        out.resize(out.size() - 1);
        return out;
    };
    auto narrow = [](const std::wstring &text) {
        std::string out(static_cast<size_t>(WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr)), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, out.data(), static_cast<int>(out.size()), nullptr, nullptr);
        out.resize(out.size() - 1);
        return out;
    };
    // Filter: "GPX files (*.gpx)\0*.gpx\0\0".
    const std::wstring pattern = L"*" + wide(extension);
    std::wstring filter = pattern + L" files";
    filter.push_back(L'\0');
    filter += pattern;
    filter.push_back(L'\0');
    filter.push_back(L'\0');
    const std::wstring title = wide(prompt);
    std::vector<wchar_t> buffer(65536, L'\0');
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof dialog;
    dialog.lpstrFilter = filter.c_str();
    dialog.lpstrFile = buffer.data();
    dialog.nMaxFile = static_cast<DWORD>(buffer.size());
    dialog.lpstrTitle = title.c_str();
    dialog.Flags = OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    std::vector<std::string> files;
    if (!GetOpenFileNameW(&dialog))
        return files;  // cancelled
    std::vector<std::wstring> parts;
    for (const wchar_t *part = buffer.data(); *part; part += wcslen(part) + 1)
        parts.emplace_back(part);
    if (parts.size() == 1)
        files.push_back(narrow(parts[0]));
    else
        for (size_t k = 1; k < parts.size(); ++k)
            files.push_back(narrow(parts[0] + L"\\" + parts[k]));
    std::vector<std::string> kept;
    for (const std::string &file : files)
        if (EndsWith(file, extension))
            kept.push_back(file);
    return kept;
}

std::string FileBrowserMissing()
{
    return "";
}
#else
std::vector<std::string> ChooseFiles(const std::string &prompt, const std::string &extension)
{
#if defined(__APPLE__)
    // AppleScript's standard dialog; a cancel ends the script with an
    // error, so nothing is printed. The prompt carries no quotes.
    const std::string command = "osascript -e 'set chosen to choose file with prompt \"" + prompt +
                                "\" with multiple selections allowed' -e 'set out to \"\"' "
                                "-e 'repeat with f in chosen' -e 'set out to out & POSIX path of f & linefeed' "
                                "-e 'end repeat' -e 'return out' 2>/dev/null";
#else
    // Linux and the BSDs: zenity (GNOME, and most desktops), else kdialog
    // (KDE). A cancel prints nothing.
    const std::string command =
        "if command -v zenity >/dev/null 2>&1; then zenity --file-selection --multiple --separator='\n' --title='" +
        prompt + "' --file-filter='*" + extension + "' 2>/dev/null; else kdialog --title '" + prompt +
        "' --getopenfilename --multiple --separate-output \"$HOME\" '*" + extension + "' 2>/dev/null; fi";
#endif
    std::vector<std::string> files;
    for (const std::string &line : Lines(command))
        if (EndsWith(line, extension))
            files.push_back(line);
    return files;
}

std::string FileBrowserMissing()
{
#if defined(__APPLE__)
    return "";
#else
    return Lines("command -v zenity || command -v kdialog").empty()
               ? "No file browser found: install zenity (or kdialog) to import files."
               : "";
#endif
}
#endif

} // namespace ui
