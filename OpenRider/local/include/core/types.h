// Activity types. The taxonomy is FIT sport/sub_sport plus an optional
// type tag for what FIT lacks (e.g. roller ski). Each external source has a
// correspondence table mapping its own type keys onto that taxonomy.
#pragma once

#include "core/result.h"

#include <map>
#include <string>
#include <vector>

namespace core {

struct ActivityType {
    std::string sport;      // FIT sport, e.g. "alpine_skiing"
    std::string sub_sport;  // FIT sub_sport, e.g. "backcountry"
    std::string tag;        // type tag, "" if none (separate from user tags)

    std::string key() const;  // "sport/sub_sport[+tag]"
    bool operator==(const ActivityType &o) const
    {
        return sport == o.sport && sub_sport == o.sub_sport && tag == o.tag;
    }
};

struct TypeLabel {
    std::string en;
    std::string fr;
    std::string icon;  // UTF-8 icon glyph (Phosphor font codepoint), "" if none
    unsigned int rgb = 0;  // display colour 0xRRGGBB; 0 = none
};

class TypeCatalog {
public:
    // activity_types.json: labels per FIT sport/sub_sport[+tag].
    Status load_labels(const std::string &json_text);
    // <source>.json: { "<source type key>": {"sport":..,"sub_sport":..,"tag":..} }
    Status load_source(const std::string &source, const std::string &json_text);

    // The source's mapping wins when known; otherwise the device's FIT type.
    ActivityType resolve(const std::string &source, const std::string &source_type,
                         const std::string &fit_sport, const std::string &fit_sub_sport) const;

    // Display label in `lang` ("en"/"fr"); falls back to the sport's label,
    // then to the raw key.
    std::string label(const ActivityType &t, const std::string &lang) const;

    // Icon glyph (UTF-8) for the type, falling back to its sport's, then a
    // generic person.
    std::string icon(const ActivityType &t) const;

    // Display colour 0xRRGGBB for the type, falling back to its sport's,
    // then grey.
    unsigned int colour(const ActivityType &t) const;

private:
    std::map<std::string, TypeLabel> labels_;  // by ActivityType::key(), or sport alone
    std::map<std::string, std::map<std::string, ActivityType>> sources_;
};

} // namespace core
