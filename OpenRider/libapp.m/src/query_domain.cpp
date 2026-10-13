#include "app/query_domain.h"

#include <set>
#include <string>

namespace app {

bool QueryDomain::Handles(IntentKind kind) const
{
    return kind == IntentKind::SetTypeFilter || kind == IntentKind::SetDateFrom || kind == IntentKind::SetDateTo ||
           kind == IntentKind::SetOnlyVisible || kind == IntentKind::SetVisibleIds;
}

void QueryDomain::Handle(const Intent &intent, Intents &out)
{
    OutingQuery &query = model_.query;
    switch (intent.kind) {
    case IntentKind::SetTypeFilter:
        query.type_key = intent.text;
        break;
    case IntentKind::SetDateFrom:
        query.from_date = intent.text;
        break;
    case IntentKind::SetDateTo:
        query.to_date = intent.text;
        break;
    case IntentKind::SetOnlyVisible:
        query.only_visible = intent.i != 0;
        if (query.only_visible)
            out.Push(IntentKind::SelectOuting, -1);  // the extent is the overview map's
        break;
    case IntentKind::SetVisibleIds:
        query.visible_ids = std::set<std::string>(intent.ids.begin(), intent.ids.end());
        break;
    default:
        break;
    }
}

} // namespace app
