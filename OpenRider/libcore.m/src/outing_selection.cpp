#include "core/outing_selection.h"

#include <algorithm>
#include <cmath>
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
    const bool numeric = key == SortKey::Distance || key == SortKey::Duration || key == SortKey::Ascent;
    auto value = [&](size_t i) {
        const or_outing &outing = outings[i].raw;
        return key == SortKey::Distance ? outing.distance_m
               : key == SortKey::Duration ? outing.duration_s
                                          : outing.ascent_m;
    };
    auto text_less = [&](size_t a, size_t b) {
        const or_outing &x = outings[a].raw;
        const or_outing &y = outings[b].raw;
        if (key == SortKey::Name)
            return std::strcmp(x.name, y.name) < 0;
        if (key == SortKey::Type)
            return outings[a].type.Key() < outings[b].type.Key();
        return std::strcmp(x.start_local, y.start_local) < 0;
    };
    std::stable_sort(selected.begin(), selected.end(), [&](size_t a, size_t b) {
        if (!numeric)
            return ascending ? text_less(a, b) : text_less(b, a);
        // A value the source does not have is NaN (input ABI). Outings
        // without one sort last whatever the direction: a NaN compared with
        // < would break the strict weak ordering std::stable_sort requires
        // (undefined behaviour).
        const double x = value(a), y = value(b);
        if (std::isnan(x) || std::isnan(y))
            return !std::isnan(x) && std::isnan(y);
        return ascending ? x < y : y < x;
    });
    return selected;
}

} // namespace core
