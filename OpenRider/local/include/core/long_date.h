/// @file
/// Dates as shown to the user (module-level utilities).
#pragma once

#include <string>

namespace core {

/// Long form of a local start time: "2026-10-12 15:53:03" -> "12 October
/// 2026" (en) / "12 octobre 2026" (fr).
std::string LongDate(const char *start_local, const std::string &lang);

} // namespace core
