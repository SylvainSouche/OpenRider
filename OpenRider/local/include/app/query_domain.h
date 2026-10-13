/// @file
/// The list query domain: which outings are listed and drawn on the
/// overview.
#pragma once

#include "app/app_model.h"
#include "app/model_domain.h"

namespace app {

/// Changes AppModel::query: type and date filters, the overview map's
/// visible outings, and whether the list follows them.
class QueryDomain : public ModelDomain {
public:
    /// @param model the model; must outlive the domain
    explicit QueryDomain(AppModel &model) : model_(model) {}

    /// SetTypeFilter, SetDateFrom, SetDateTo, SetOnlyVisible, SetVisibleIds.
    bool Handles(IntentKind kind) const override;
    void Handle(const Intent &intent, Intents &out) override;

private:
    AppModel &model_;  ///< the model this domain changes
};

} // namespace app
