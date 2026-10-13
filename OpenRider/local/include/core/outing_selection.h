/// @file
/// Filtering and sorting of library outings (module-level utilities).
#pragma once

#include "core/library.h"

#include <set>
#include <string>
#include <vector>

namespace core {

/// Sort order of the outing list.
enum class SortKey { Date, Name, Type, Distance, Duration, Ascent };

/// Which outings to keep; empty fields do not filter.
struct OutingFilter {
    std::string type_key;   ///< ActivityType::Key(), "" = all
    std::string from_date;  ///< "YYYY-MM-DD", "" = open
    std::string to_date;    ///< "YYYY-MM-DD", inclusive, "" = open
    const std::set<std::string> *only_ids = nullptr;  ///< e.g. tracks visible on the map; null = all
};

/// The outing passes every criterion of the filter.
bool OutingMatches(const Outing &outing, const OutingFilter &filter);

/// Indices of `outings` matching `filter`, ordered by `key` (stable; ties by date).
std::vector<size_t> SelectOutings(const std::vector<Outing> &outings,
                                  const OutingFilter &filter, SortKey key, bool ascending);

} // namespace core
