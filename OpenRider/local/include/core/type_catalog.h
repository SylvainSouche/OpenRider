/// @file
/// The catalog of activity types: display labels, icons and colours, plus
/// one correspondence table per external source mapping its own type keys
/// onto the OpenRider taxonomy.
#pragma once

#include "core/activity_type.h"
#include "core/result.h"

#include <map>
#include <string>

namespace core {

/// How an activity type is displayed.
struct TypeLabel {
    std::string en;        ///< English label
    std::string fr;        ///< French label
    std::string icon;      ///< UTF-8 icon glyph (Phosphor font codepoint), "" if none
    unsigned int rgb = 0;  ///< display colour 0xRRGGBB; 0 = none
};

/// Activity type labels and source correspondence tables, loaded from the
/// JSON files of share/openrider/types.
class TypeCatalog {
public:
    /// Loads activity_types.json: labels per FIT sport/sub_sport[+tag].
    /// @param json_text the file's content
    Status LoadLabels(const std::string &json_text);

    /// Loads a source's correspondence table, `<source>.json`:
    /// { "<source type key>": {"sport":..,"sub_sport":..,"tag":..} }.
    /// @param source source name, e.g. "garmin_connect"
    /// @param json_text the file's content
    Status LoadSource(const std::string &source, const std::string &json_text);

    /// Type of an outing: the source's mapping of `source_type` wins when
    /// known; otherwise the device's FIT sport/sub_sport.
    ActivityType Resolve(const std::string &source, const std::string &source_type,
                         const std::string &fit_sport, const std::string &fit_sub_sport) const;

    /// Display label in `lang` ("en"/"fr"); falls back to the sport's label,
    /// then to the raw key.
    std::string Label(const ActivityType &type, const std::string &lang) const;

    /// Icon glyph (UTF-8) for the type, falling back to its sport's, then a
    /// generic person.
    std::string Icon(const ActivityType &type) const;

    /// Display colour 0xRRGGBB for the type, falling back to its sport's,
    /// then grey.
    unsigned int Colour(const ActivityType &type) const;

private:
    std::map<std::string, TypeLabel> labels_;  ///< by ActivityType::Key(), or sport alone
    std::map<std::string, std::map<std::string, ActivityType>> sources_;  ///< source -> its key -> type
};

} // namespace core
