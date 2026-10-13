/// @file
/// Intents: what views report (user actions) and controllers emit to each
/// other. A view never changes state; it pushes intents, and the dispatcher
/// hands each one to the single controller owning that kind.
#pragma once

#include "core/outing_selection.h"
#include "core/series.h"

#include <string>
#include <vector>

namespace app {

/// What an intent asks for, grouped by owning controller. The comment of
/// each kind says which Intent fields it uses.
enum class IntentKind {
    // AppController (the model)
    SelectOuting,        ///< i: outing index, -1 = none (overview)
    SetTypeFilter,       ///< text: ActivityType key, "" = all
    SetDateFrom,         ///< text: "YYYY-MM-DD" (possibly partial)
    SetDateTo,           ///< text
    SetOnlyVisible,      ///< i: 0/1 -- list restricted to tracks visible on the overview
    SetVisibleIds,       ///< ids: outings visible on the overview map
    SetShowSegments,     ///< i: 0/1 -- overview shows the segment networks instead of the tracks
    SelectSegmentMethod, ///< i: segments plugin index
    SetSegmentParam,     ///< i: parameter index of the selected method, x: value
    OpenSegmentComparison,  ///< i: segment group index, j: segment id
    CloseSegmentComparison, ///< back from the comparison panel
    ToggleFilter,        ///< i: filter index, j: 0/1
    SetFilterParam,      ///< i: filter index, j: parameter index, x: value
    ResetFilterParams,   ///< i: filter index
    DismissError,        ///< drops the error shown
    ReportError,         ///< text
    Quit,                ///< ends the application

    // ListViewController
    SortBy,              ///< i: core::SortKey
    ToggleSortDirection, ///< ascending <-> descending

    // MapViewController
    MapViewport,         ///< x, y: map region size in pixels
    MapPan,              ///< x, y: pixels dragged
    MapZoom,             ///< i: +1/-1, x, y: mouse offset from the map centre
    MapMouse,            ///< x, y: mouse offset from the map centre; i: 1 inside, 0 outside
    MapClick,            ///< a click (no drag) on the map
    MapDoubleClick,      ///< a double click on the map
    SelectLayer,         ///< i: layer index
    SetTrackStyle,       ///< i: track style index
    SetTrackColour,      ///< x: packed RGBA as a double (0..2^32)

    // OutingViewController
    SetXAxis,            ///< i: core::XAxis
    AddGraph,            ///< appends a graph
    RemoveGraph,         ///< i: graph index
    SetGraphSpec,        ///< i: graph index, spec
    GraphHover,          ///< i: graph index, x: value on the x axis
    GraphHoverEnd,       ///< the mouse left the graphs
    SetStatsComputed,    ///< i: 0 Garmin, 1 computed

    // ComparisonViewController
    ToggleComparedRun,       ///< i: run index
    ShowAllComparedRuns,     ///< i: 1 all, 0 none
    SetComparisonXAxis,      ///< i: core::XAxis
    AddComparisonGraph,      ///< appends a graph
    RemoveComparisonGraph,   ///< i: graph index
    SetComparisonGraphSpec,  ///< i: graph index, spec

    // MainViewController
    ResizeList,          ///< x: pixels
    ResizeMap,           ///< y: pixels
    ShowAbout,           ///< i: 0/1
};

/// One intent: its kind plus a generic payload whose meaning the kind
/// defines (see IntentKind). The payload fields are deliberately generic,
/// like the arguments of a message.
struct Intent {
    /// An intent of `kind` with an empty payload.
    explicit Intent(IntentKind kind) : kind(kind) {}

    IntentKind kind;               ///< what is asked
    int i = 0;                     ///< first integer argument (index, flag, enum value)
    int j = 0;                     ///< second integer argument
    double x = 0.0;                ///< first real argument (pixels, values)
    double y = 0.0;                ///< second real argument
    std::string text;              ///< text argument
    std::vector<std::string> ids;  ///< outing ids argument
    core::SeriesSpec spec;         ///< graph specification argument
};

/// Intents collected during a frame and dispatched afterwards; controllers
/// push their follow-ups to the same queue.
class Intents {
public:
    /// Queues a fully built intent.
    void Push(Intent intent);

    /// Queues an intent with numeric arguments.
    void Push(IntentKind kind, int i = 0, int j = 0, double x = 0.0, double y = 0.0);

    /// Queues an intent with a text argument.
    void PushText(IntentKind kind, std::string text);

    /// Nothing is queued.
    bool Empty() const { return items_.empty(); }

    /// Takes every queued intent, oldest first, leaving the queue empty.
    std::vector<Intent> Take();

private:
    std::vector<Intent> items_;  ///< oldest first
};

} // namespace app
