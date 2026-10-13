/// @file
/// OpenRider views (Dear ImGui). Views are read-only: they draw from the
/// model and the view models (const) and report user actions as intents.
/// Even ImGui's editable widgets work on a local copy; a change becomes an
/// intent, applied by its controller before the next frame. Module-level
/// functions, one source file per view.
#pragma once

#include "style.h"
#include "texture_mirror.h"

#include "app/dispatch.h"
#include "app/graphs.h"
#include "app/map_view_utils.h"

namespace ui {

/// Every view model, read-only, as the views see them.
struct ViewModels {
    const app::MainViewModel &main;      ///< layout
    const app::ListViewModel &list;      ///< outing list
    const app::MapViewModel &map;        ///< map
    const app::OutingViewModel &outing;  ///< outing panel
    const app::ComparisonViewModel &comparison;  ///< segment comparison panel
};

/// The main window: menu bar, outing list and right-hand panel side by
/// side, status bar, About box, error popup.
void DrawMainView(const app::AppModel &model, const ViewModels &view_models, TextureMirror &textures,
                  app::Intents &out);

/// The outing list: query widgets, sort controls, one row per outing.
void DrawListView(const app::AppModel &model, const ViewModels &view_models, app::Intents &out);

/// Right-hand panel with no outing selected: every listed track on the map.
void DrawOverviewView(const app::AppModel &model, const ViewModels &view_models, TextureMirror &textures,
                      app::Intents &out);

/// Right-hand panel for the selected outing: statistics, filters, map, graphs.
void DrawOutingView(const app::AppModel &model, const ViewModels &view_models, TextureMirror &textures,
                    app::Intents &out);

/// Right-hand panel comparing the outings along a segment: the passages to
/// show and the map on the first row, the graphs below, one curve per
/// passage.
void DrawComparisonView(const app::AppModel &model, const ViewModels &view_models, TextureMirror &textures,
                        app::Intents &out);

/// The intents graph controls push: they belong to the panel showing them.
struct GraphIntents {
    app::IntentKind set_xaxis;  ///< i: openrider::sdk::XAxis
    app::IntentKind add;        ///< appends a graph
    app::IntentKind remove;     ///< i: graph index
    app::IntentKind set_spec;   ///< i: graph index, spec
};

/// The x axis choice, an "add graph" button and one row of controls per
/// graph (series, type, window/unit, remove).
void DrawGraphControls(const std::vector<openrider::sdk::SeriesSpec> &graphs, openrider::sdk::XAxis xaxis, const GraphIntents &intents,
                       app::Intents &out);

/// The map, `width` x `height` pixels at the cursor: tiles, tracks,
/// attribution; mouse actions reported as intents.
void DrawMapView(const app::AppModel &model, const ViewModels &view_models, TextureMirror &textures,
                 float width, float height, app::Intents &out);

} // namespace ui
