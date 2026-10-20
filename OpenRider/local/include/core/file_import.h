/// @file
/// Copying files into a source's folder (module-level utilities).
// @impl 0f87-6ad7-2a41-a86a
#pragma once

#include <string>

namespace core {

/// Copies `file` into folder `dir` under its own name, or with " (2)",
/// " (3)"... before the extension when that name is taken.
/// @return the copy's path; "" on failure, with `error` set
std::string CopyIntoFolder(const std::string &file, const std::string &dir, std::string &error);

} // namespace core
