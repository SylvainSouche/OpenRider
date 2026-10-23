#include "core/outing_types.h"

#include <cJSON.h>

#include <cstdio>
#include <fstream>
#include <sstream>

namespace core {

namespace {

std::string Text(const cJSON *item, const char *name)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(item, name);
    return cJSON_IsString(v) ? v->valuestring : "";
}

} // namespace

ActivityType TypeOfKey(const std::string &key)
{
    ActivityType type;
    const size_t plus = key.find('+');
    const std::string base = key.substr(0, plus);
    if (plus != std::string::npos)
        type.tag = key.substr(plus + 1);
    const size_t slash = base.find('/');
    type.sport = base.substr(0, slash);
    type.sub_sport = slash == std::string::npos ? "generic" : base.substr(slash + 1);
    return type;
}

openrider::sdk::Status OutingTypes::Load(const std::string &path)
{
    path_ = path;
    types_.clear();
    std::ifstream in(path);
    if (!in)
        return openrider::sdk::Status::Success();  // none settled yet
    std::stringstream text;
    text << in.rdbuf();
    cJSON *root = cJSON_Parse(text.str().c_str());
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return openrider::sdk::Status::Failure(path + ": invalid JSON");
    }
    const cJSON *item = nullptr;
    cJSON_ArrayForEach(item, root)
    {
        ActivityType type{Text(item, "sport"), Text(item, "sub_sport"), Text(item, "tag")};
        if (!type.sport.empty())
            types_[item->string] = type;
    }
    cJSON_Delete(root);
    return openrider::sdk::Status::Success();
}

openrider::sdk::Status OutingTypes::Save() const
{
    cJSON *root = cJSON_CreateObject();
    for (const auto &[id, type] : types_) {
        cJSON *item = cJSON_AddObjectToObject(root, id.c_str());
        cJSON_AddStringToObject(item, "sport", type.sport.c_str());
        cJSON_AddStringToObject(item, "sub_sport", type.sub_sport.c_str());
        if (!type.tag.empty())
            cJSON_AddStringToObject(item, "tag", type.tag.c_str());
    }
    char *text = cJSON_Print(root);
    const std::string temporary = path_ + ".tmp";
    bool written = false;
    {
        std::ofstream out(temporary, std::ios::trunc);
        out << (text ? text : "{}") << "\n";
        written = static_cast<bool>(out);
    }
    cJSON_free(text);
    cJSON_Delete(root);
    if (!written || std::rename(temporary.c_str(), path_.c_str()) != 0) {
        std::remove(temporary.c_str());
        return openrider::sdk::Status::Failure("cannot write " + path_);
    }
    return openrider::sdk::Status::Success();
}

bool OutingTypes::Find(const std::string &id, ActivityType &type) const
{
    const auto found = types_.find(id);
    if (found == types_.end())
        return false;
    type = found->second;
    return true;
}

} // namespace core
