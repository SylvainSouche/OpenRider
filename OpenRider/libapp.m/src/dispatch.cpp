#include "app/dispatch.h"

namespace app {

namespace {

// Controllers owning intent kinds.
enum class Owner { App, Main, List, Map, Outing, Comparison };

// The controller owning intents of `kind`.
Owner OwnerOf(IntentKind kind)
{
    switch (kind) {
    case IntentKind::SortBy:
    case IntentKind::ToggleSortDirection:
        return Owner::List;
    case IntentKind::MapViewport:
    case IntentKind::MapPan:
    case IntentKind::MapZoom:
    case IntentKind::MapMouse:
    case IntentKind::MapClick:
    case IntentKind::MapDoubleClick:
    case IntentKind::SelectLayer:
    case IntentKind::SetTrackStyle:
    case IntentKind::SetTrackColour:
        return Owner::Map;
    case IntentKind::SetXAxis:
    case IntentKind::AddGraph:
    case IntentKind::RemoveGraph:
    case IntentKind::SetGraphSpec:
    case IntentKind::GraphHover:
    case IntentKind::GraphHoverEnd:
    case IntentKind::SetStatsComputed:
        return Owner::Outing;
    case IntentKind::ToggleComparedRun:
    case IntentKind::ShowAllComparedRuns:
    case IntentKind::SetComparisonXAxis:
    case IntentKind::AddComparisonGraph:
    case IntentKind::RemoveComparisonGraph:
    case IntentKind::SetComparisonGraphSpec:
        return Owner::Comparison;
    case IntentKind::ResizeList:
    case IntentKind::ResizeMap:
    case IntentKind::ShowAbout:
        return Owner::Main;
    default:
        return Owner::App;
    }
}

} // namespace

// @impl 0f87-6acd-ce40-f1bc
void Dispatch(Controllers &controllers, Intents &intents)
{
    // Follow-ups land in the same queue; a bound keeps a feedback loop
    // between controllers from spinning forever.
    for (int round = 0; round < 8 && !intents.Empty(); ++round) {
        for (const Intent &intent : intents.Take()) {
            switch (OwnerOf(intent.kind)) {
            case Owner::App: controllers.app.Handle(intent, intents); break;
            case Owner::Main: controllers.main.Handle(intent); break;
            case Owner::List: controllers.list.Handle(intent); break;
            case Owner::Map: controllers.map.Handle(intent, controllers.model, intents); break;
            case Owner::Outing: controllers.outing.Handle(intent, controllers.model); break;
            case Owner::Comparison: controllers.comparison.Handle(intent); break;
            }
        }
    }
}

void Step(Controllers &controllers, Intents &intents)
{
    Dispatch(controllers, intents);  // what the views reported, so the ticks see it
    controllers.app.Tick(intents);
    controllers.outing.Tick(controllers.model);
    controllers.comparison.Tick(controllers.model);
    controllers.map.Tick(controllers.model, intents);
    Dispatch(controllers, intents);  // what the ticks emitted
}

} // namespace app
