/// @file
/// The selected outing's presentation: graphs, x axis, hover, statistics
/// source.
#pragma once

#include "app/app_model.h"
#include "app/intents.h"

#include "core/series.h"

#include <string>
#include <vector>

namespace app {

/// What the outing view keeps of its presentation.
struct OutingViewModel {
    core::XAxis xaxis = core::XAxis::Time;                        ///< shared by every graph
    std::vector<core::SeriesSpec> graphs = {core::SeriesSpec{}};  ///< stacked graphs; starts with one
    int hover_index = -1;          ///< point under the mouse in a graph, -1 = none
    double hover_x = 0.0;          ///< its x value (cursor line)
    bool stats_computed = false;   ///< statistics source when no filter is on: computed, not Garmin's
};

/// Owns the OutingViewModel and changes it in response to graph intents.
class OutingViewController {
public:
    /// The view model, for the outing view.
    const OutingViewModel &ViewModel() const { return view_model_; }

    /// Follows the model: a newly loaded outing resets the hover.
    void Tick(const AppModel &model);

    /// Handles SetXAxis, AddGraph, RemoveGraph, SetGraphSpec, GraphHover,
    /// GraphHoverEnd and SetStatsComputed.
    void Handle(const Intent &intent, const AppModel &model);

private:
    OutingViewModel view_model_;  ///< changed only here
    std::string points_id_;  ///< outing whose points the hover refers to
};

} // namespace app
