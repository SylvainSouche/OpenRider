/// @file
/// The session domain: errors shown to the user, and quitting.
#pragma once

#include "app/app_model.h"
#include "app/model_domain.h"

namespace app {

/// Queues and dismisses errors, and ends the session.
class SessionDomain : public ModelDomain {
public:
    /// @param model the model; must outlive the domain
    explicit SessionDomain(AppModel &model) : model_(model) {}

    /// ReportError, DismissError, Quit.
    bool Handles(IntentKind kind) const override;
    void Handle(const Intent &intent, Intents &out) override;

private:
    AppModel &model_;  ///< the model this domain changes
};

} // namespace app
