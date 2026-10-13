/// @file
/// The dispatcher: routes every intent to the single controller owning its
/// kind, until no follow-up intent remains. Views push intents during a
/// frame; main calls Step() once per frame. Module-level utilities.
#pragma once

#include "app/app_controller.h"
#include "app/comparison_view_controller.h"
#include "app/list_view_controller.h"
#include "app/main_view_controller.h"
#include "app/map_view_controller.h"
#include "app/outing_view_controller.h"

namespace app {

/// The model and every controller, as the dispatcher sees them.
struct Controllers {
    AppModel &model;               ///< the application model
    AppController &app;            ///< owner of the model
    MainViewController &main;      ///< owner of MainViewModel
    ListViewController &list;      ///< owner of ListViewModel
    MapViewController &map;        ///< owner of MapViewModel
    OutingViewController &outing;  ///< owner of OutingViewModel
    ComparisonViewController &comparison;  ///< owner of ComparisonViewModel
};

/// One frame: pending intents first (the views' reports), then background
/// progress (ticks), then whatever the ticks emitted -- follow-ups included.
void Step(Controllers &controllers, Intents &intents);

/// Hands every queued intent to its owner, follow-ups included (bounded to
/// a few rounds).
void Dispatch(Controllers &controllers, Intents &intents);

} // namespace app
