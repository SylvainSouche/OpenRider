// Routing of the model's intents: each handled by exactly one domain.

#include <atf-c++.hpp>

#include "app/comparison_domain.h"
#include "app/input_source.h"
#include "app/library_domain.h"
#include "app/outing_domain.h"
#include "app/query_domain.h"
#include "app/segments_domain.h"
#include "app/session_domain.h"

#include <vector>

ATF_TEST_CASE_WITHOUT_HEAD(each_model_intent_has_one_domain);
ATF_TEST_CASE_BODY(each_model_intent_has_one_domain)
{
    app::AppModel model;
    app::InputSource input;
    app::LibraryDomain library(model, input);  // input: non-const, the scan assigns outings to sources
    app::QueryDomain query(model);
    app::OutingDomain outing(model, input);
    app::SegmentsDomain segments(model, input);
    app::ComparisonDomain comparison(model, input);
    app::SessionDomain session(model);
    const std::vector<const app::ModelDomain *> domains = {&library, &query, &outing, &segments, &comparison, &session};
    auto handlers = [&](app::IntentKind kind) {
        int count = 0;
        for (const app::ModelDomain *domain : domains)
            count += domain->Handles(kind) ? 1 : 0;
        return count;
    };
    // The model's intents (the first group of IntentKind).
    const app::IntentKind model_kinds[] = {
        app::IntentKind::SelectOuting,        app::IntentKind::SetTypeFilter,         app::IntentKind::SetDateFrom,
        app::IntentKind::SetDateTo,           app::IntentKind::SetOnlyVisible,        app::IntentKind::SetVisibleIds,
        app::IntentKind::SetShowSegments,     app::IntentKind::SelectSegmentMethod,   app::IntentKind::SetSegmentParam,
        app::IntentKind::OpenSegmentComparison, app::IntentKind::CloseSegmentComparison, app::IntentKind::ToggleFilter,
        app::IntentKind::SetFilterParam,      app::IntentKind::ResetFilterParams,     app::IntentKind::ReloadOverview,
        app::IntentKind::SetOutingType,       app::IntentKind::ConfirmOutingTypes,    app::IntentKind::DismissError,
        app::IntentKind::ReportError,         app::IntentKind::Quit};
    int model_count = 0;
    for (app::IntentKind kind : model_kinds) {
        ATF_REQUIRE_EQ(handlers(kind), 1);
        ++model_count;
    }
    // No domain takes a view controller's intent.
    int handled = 0;
    for (int kind = 0; kind <= static_cast<int>(app::IntentKind::ShowAbout); ++kind)
        handled += handlers(static_cast<app::IntentKind>(kind)) > 0 ? 1 : 0;
    ATF_REQUIRE_EQ(handled, model_count);
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, each_model_intent_has_one_domain);
}
