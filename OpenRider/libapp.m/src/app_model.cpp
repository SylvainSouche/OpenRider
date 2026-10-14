#include "app/app_model.h"

namespace app {

bool AppModel::AnyFilterOn() const
{
    for (const FilterState &filter : filters)
        if (filter.enabled)
            return true;
    return false;
}

core::OutingFilter AppModel::MapFilter() const
{
    core::OutingFilter filter;
    filter.type_key = query.type_key;
    filter.from_date = query.from_date;
    filter.to_date = query.to_date;
    return filter;
}

std::vector<core::TrackFilter> AppModel::EnabledFilters() const
{
    std::vector<core::TrackFilter> enabled;
    for (const FilterState &filter : filters)
        if (filter.enabled)
            enabled.push_back({filter.vtable, filter.values});
    return enabled;
}

core::OutingFilter AppModel::ListFilter() const
{
    core::OutingFilter filter = MapFilter();
    if (query.only_visible)
        filter.only_ids = &query.visible_ids;
    return filter;
}

const core::Outing *AppModel::SelectedOuting() const
{
    return selected >= 0 && selected < static_cast<int>(outings.size())
               ? &outings[static_cast<size_t>(selected)]
               : nullptr;
}

} // namespace app
