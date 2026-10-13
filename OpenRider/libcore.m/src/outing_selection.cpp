#include "core/outing_selection.h"

#include <algorithm>
#include <cstring>

namespace core {

bool OutingMatches(const Outing &outing, const OutingFilter &filter)
{
    if (!filter.type_key.empty() && outing.type.Key() != filter.type_key)
        return false;
    if (filter.only_ids && filter.only_ids->count(outing.raw.id) == 0)
        return false;
    const std::string day(outing.raw.start_local, std::min<size_t>(10, std::strlen(outing.raw.start_local)));
    if (!filter.from_date.empty() && day < filter.from_date)
        return false;
    if (!filter.to_date.empty() && day > filter.to_date)
        return false;
    return true;
}

// @impl 0f87-6acd-24ce-ac37
std::vector<size_t> SelectOutings(const std::vector<Outing> &outings, const OutingFilter &filter,
                                   SortKey key, bool ascending)
{
    std::vector<size_t> selected;
    for (size_t i = 0; i < outings.size(); ++i)
        if (OutingMatches(outings[i], filter))
            selected.push_back(i);
    auto less = [&](size_t a, size_t b) {
        const or_outing &x = outings[a].raw;
        const or_outing &y = outings[b].raw;
        switch (key) {
        case SortKey::Name: return std::strcmp(x.name, y.name) < 0;
        case SortKey::Type: return outings[a].type.Key() < outings[b].type.Key();
        case SortKey::Distance: return x.distance_m < y.distance_m;
        case SortKey::Duration: return x.duration_s < y.duration_s;
        case SortKey::Ascent: return x.ascent_m < y.ascent_m;
        case SortKey::Date: break;
        }
        return std::strcmp(x.start_local, y.start_local) < 0;
    };
    std::stable_sort(selected.begin(), selected.end(), [&](size_t a, size_t b) {
        return ascending ? less(a, b) : less(b, a);
    });
    return selected;
}

} // namespace core
