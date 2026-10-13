#include "app/list_view_controller.h"

namespace app {

void ListViewController::Handle(const Intent &intent)
{
    switch (intent.kind) {
    case IntentKind::SortBy:
        view_model_.sort_key = static_cast<core::SortKey>(intent.i);
        break;
    case IntentKind::ToggleSortDirection:
        view_model_.ascending = !view_model_.ascending;
        break;
    default:
        break;
    }
}

} // namespace app
