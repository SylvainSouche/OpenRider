#include "core/file_import.h"

#include <filesystem>
#include <system_error>

namespace core {

std::string CopyIntoFolder(const std::string &file, const std::string &dir, std::string &error)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path source(file);
    if (!fs::is_regular_file(source, ec)) {
        error = "no file " + file;
        return "";
    }
    // Joined with '/', as every OpenRider path: fs::path's operator/ would put
    // '\' on Windows (dir + "\run.gpx"), unlike the rest of the path.
    fs::path target = dir + "/" + source.filename().string();
    for (int n = 2; fs::exists(target, ec); ++n)
        target = dir + "/" + source.stem().string() + " (" + std::to_string(n) + ")" + source.extension().string();
    if (!fs::copy_file(source, target, ec)) {
        error = "cannot copy " + file + " to " + dir + ": " + ec.message();
        return "";
    }
    return target.string();
}

} // namespace core
