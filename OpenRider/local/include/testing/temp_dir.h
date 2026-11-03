/// @file
/// Test support: a fresh temporary directory, portably (mkdtemp is POSIX
/// only; MSVC has none). For the test programs of every module, plugins
/// included; never linked into a shipped library or program.
#pragma once

#include <filesystem>
#include <random>
#include <string>
#include <system_error>

namespace testing {

/// Creates a new, empty directory under the system's temporary directory,
/// named "openrider-<what>-<random>", and returns its path with '/'
/// separators; "" if none could be created.
inline std::string TempDir(const std::string &what)
{
    std::error_code ec;
    const std::filesystem::path base = std::filesystem::temp_directory_path(ec);
    if (ec)
        return "";
    std::random_device seed;
    std::mt19937_64 random(seed());
    for (int attempt = 0; attempt < 100; ++attempt) {
        const std::filesystem::path dir =
            base / ("openrider-" + what + "-" + std::to_string(random() % 1000000000000ull));
        if (std::filesystem::create_directory(dir, ec))
            return dir.generic_string();
    }
    return "";
}

} // namespace testing
