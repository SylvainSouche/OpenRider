/// @file
/// Activity type: FIT sport/sub_sport plus an optional type tag for what FIT
/// lacks (e.g. roller ski).
#pragma once

#include <string>

namespace core {

/// One activity type of the OpenRider taxonomy.
struct ActivityType {
    std::string sport;      ///< FIT sport, e.g. "alpine_skiing"
    std::string sub_sport;  ///< FIT sub_sport, e.g. "backcountry"
    std::string tag;        ///< type tag, "" if none (separate from user tags)

    /// Catalog key of the type: "sport/sub_sport[+tag]".
    std::string Key() const;

    /// Same sport, sub_sport and tag.
    bool operator==(const ActivityType &o) const
    {
        return sport == o.sport && sub_sport == o.sub_sport && tag == o.tag;
    }
};

} // namespace core
