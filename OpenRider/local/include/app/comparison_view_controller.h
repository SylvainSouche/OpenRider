/// @file
/// The segment comparison panel's presentation: which passages are shown,
/// the graphs and their x axis.
#pragma once

#include "app/app_model.h"
#include "app/intents.h"

#include <openrider/sdk/series.h>

#include <vector>

namespace app {

/// What the comparison panel keeps of its presentation.
struct ComparisonViewModel {
    std::vector<bool> shown;                                      ///< per ComparedRun: drawn on the map and graphs
    openrider::sdk::XAxis xaxis = openrider::sdk::XAxis::Distance;                    ///< distance: position along the segment
    std::vector<openrider::sdk::SeriesSpec> graphs = {openrider::sdk::SeriesSpec{}};  ///< stacked graphs; starts with one
    bool hovering = false;                                        ///< the mouse is over a graph
    double hover_x = 0.0;                                         ///< its position on the x axis (km or min)
};

/// Owns the ComparisonViewModel and changes it in response to the
/// comparison panel's intents.
class ComparisonViewController {
public:
    /// The view model, for the comparison view.
    const ComparisonViewModel &ViewModel() const { return view_model_; }

    /// Follows the model: a newly opened comparison shows every passage.
    void Tick(const AppModel &model);

    /// Handles ToggleComparedRun, ShowAllComparedRuns, SetComparisonXAxis,
    /// AddComparisonGraph, RemoveComparisonGraph, SetComparisonGraphSpec,
    /// ComparisonHover and ComparisonHoverEnd.
    void Handle(const Intent &intent);

private:
    ComparisonViewModel view_model_;  ///< changed only here
    unsigned generation_ = 0;         ///< comparison the selection belongs to
};

} // namespace app
