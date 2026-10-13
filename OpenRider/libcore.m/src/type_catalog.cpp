#include "core/type_catalog.h"

#include <cJSON.h>

#include <cstdlib>

namespace core {

namespace {

std::string JsonString(const cJSON *obj, const char *key)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsString(v) ? v->valuestring : "";
}

// "e71c" -> UTF-8 encoding of U+E71C; "" if not a hex codepoint.
std::string Utf8FromHex(const std::string &hex)
{
    if (hex.empty() || hex.size() > 6)
        return "";
    char *end = nullptr;
    const unsigned long cp = std::strtoul(hex.c_str(), &end, 16);
    if (*end != '\0' || cp == 0 || cp > 0x10FFFF)
        return "";
    std::string out;
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
    return out;
}

constexpr const char *kGenericIcon = "\xee\x9c\xae";  // U+E72E person-simple

} // namespace

Status TypeCatalog::LoadLabels(const std::string &json_text)
{
    cJSON *root = cJSON_Parse(json_text.c_str());
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return Status::Failure("activity type labels: invalid JSON");
    }
    const cJSON *item = nullptr;
    cJSON_ArrayForEach(item, root) {
        const std::string col = JsonString(item, "colour");
        const unsigned int rgb =
            col.size() == 7 && col[0] == '#' ? static_cast<unsigned int>(std::strtoul(col.c_str() + 1, nullptr, 16)) : 0;
        labels_[item->string] = TypeLabel{JsonString(item, "en"), JsonString(item, "fr"),
                                          Utf8FromHex(JsonString(item, "icon")), rgb};
    }
    cJSON_Delete(root);
    return Status::Success();
}

// @impl 0f87-6acd-2587-fdb3
Status TypeCatalog::LoadSource(const std::string &source, const std::string &json_text)
{
    cJSON *root = cJSON_Parse(json_text.c_str());
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return Status::Failure(source + " type table: invalid JSON");
    }
    auto &table = sources_[source];
    const cJSON *item = nullptr;
    cJSON_ArrayForEach(item, root) {
        ActivityType t{JsonString(item, "sport"), JsonString(item, "sub_sport"),
                       JsonString(item, "tag")};
        if (t.sub_sport.empty())
            t.sub_sport = "generic";
        table[item->string] = t;
    }
    cJSON_Delete(root);
    return Status::Success();
}

ActivityType TypeCatalog::Resolve(const std::string &source, const std::string &source_type,
                                  const std::string &fit_sport,
                                  const std::string &fit_sub_sport) const
{
    if (!source_type.empty()) {
        const auto s = sources_.find(source);
        if (s != sources_.end()) {
            const auto t = s->second.find(source_type);
            if (t != s->second.end())
                return t->second;
        }
    }
    return ActivityType{fit_sport.empty() ? "generic" : fit_sport,
                        fit_sub_sport.empty() ? "generic" : fit_sub_sport, ""};
}

std::string TypeCatalog::Label(const ActivityType &type, const std::string &lang) const
{
    auto pick = [&](const TypeLabel &label) { return lang == "fr" && !label.fr.empty() ? label.fr : label.en; };
    auto it = labels_.find(type.Key());
    if (it == labels_.end() && type.tag.empty())
        it = labels_.find(type.sport);
    if (it != labels_.end() && !pick(it->second).empty())
        return pick(it->second);
    return type.Key();
}

// @impl 0f87-6acd-30f9-68bb
std::string TypeCatalog::Icon(const ActivityType &type) const
{
    for (const std::string &key : {type.Key(), type.sport}) {
        const auto it = labels_.find(key);
        if (it != labels_.end() && !it->second.icon.empty())
            return it->second.icon;
    }
    return kGenericIcon;
}

// @impl 0f87-6acd-6195-c32d
unsigned int TypeCatalog::Colour(const ActivityType &type) const
{
    for (const std::string &key : {type.Key(), type.sport}) {
        const auto it = labels_.find(key);
        if (it != labels_.end() && it->second.rgb != 0)
            return it->second.rgb;
    }
    return 0x8c8c8c;  // grey
}

} // namespace core
