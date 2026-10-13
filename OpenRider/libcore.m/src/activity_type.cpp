#include "core/activity_type.h"

namespace core {

std::string ActivityType::Key() const
{
    std::string k = sport + "/" + (sub_sport.empty() ? "generic" : sub_sport);
    if (!tag.empty())
        k += "+" + tag;
    return k;
}

} // namespace core
