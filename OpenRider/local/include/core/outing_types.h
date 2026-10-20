/// @file
/// The types the user settled for outings, kept in a file of the
/// application's own (plugins never write the application's data).
// @impl 0f87-6ad7-26b7-38bf
#pragma once

#include "core/activity_type.h"

#include <openrider/sdk/result.h>

#include <map>
#include <string>

namespace core {

/// The activity type the user confirmed or chose for each outing, by outing
/// id, kept in a JSON file ({"<id>": {"sport": ..., "sub_sport": ..., "tag": ...}}).
class OutingTypes {
public:
    /// Reads `path` (a missing file is an empty set); later saves go there.
    openrider::sdk::Status Load(const std::string &path);

    /// Writes the set to the file given to Load(), atomically.
    openrider::sdk::Status Save() const;

    /// The type settled for outing `id`.
    /// @return false if none
    bool Find(const std::string &id, ActivityType &type) const;

    /// Settles outing `id`'s type (saved by the next Save()).
    void Set(const std::string &id, const ActivityType &type) { types_[id] = type; }

private:
    std::string path_;                          ///< the file
    std::map<std::string, ActivityType> types_;  ///< by outing id
};

/// The type of a key "sport/sub_sport[+tag]" (ActivityType::Key()).
ActivityType TypeOfKey(const std::string &key);

} // namespace core
