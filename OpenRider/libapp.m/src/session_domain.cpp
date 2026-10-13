#include "app/session_domain.h"

#include "app/reports.h"

namespace app {

bool SessionDomain::Handles(IntentKind kind) const
{
    return kind == IntentKind::ReportError || kind == IntentKind::DismissError || kind == IntentKind::Quit;
}

void SessionDomain::Handle(const Intent &intent, Intents &)
{
    switch (intent.kind) {
    case IntentKind::ReportError:
        ReportError(model_, intent.text);
        break;
    case IntentKind::DismissError:
        if (!model_.errors.empty())
            model_.errors.erase(model_.errors.begin());
        break;
    case IntentKind::Quit:
        model_.quit = true;
        break;
    default:
        break;
    }
}

} // namespace app
