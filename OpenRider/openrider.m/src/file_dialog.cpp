#include "file_dialog.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace ui {

namespace {

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
}

} // namespace ui
