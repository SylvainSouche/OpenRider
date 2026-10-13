/// @file
/// The outing list's presentation: sort order.
#pragma once

#include "app/intents.h"

#include "core/outing_selection.h"

namespace app {

/// What the list view keeps of its presentation.
struct ListViewModel {
    core::SortKey sort_key = core::SortKey::Date;  ///< column sorted on
    bool ascending = false;                        ///< sort direction
};

/// Owns the ListViewModel and changes it in response to sorting intents.
class ListViewController {
public:
    /// The view model, for the list view.
    const ListViewModel &ViewModel() const { return view_model_; }

    /// Handles SortBy and ToggleSortDirection.
    void Handle(const Intent &intent);

private:
    ListViewModel view_model_;  ///< changed only here
};

} // namespace app
