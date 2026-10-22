/// @file
/// The selected outing's presentation: graphs, x axis, hover, statistics
/// source.
#pragma once

#include "app/app_model.h"
#include "app/intents.h"

#include <openrider/sdk/series.h>

#include <string>
#include <vector>

namespace app {

/// What the outing view keeps of its presentation.
struct OutingViewModel {
    openrider::sdk::XAxis xaxis = openrider::sdk::XAxis::Time;                        ///< shared by every graph
    std::vector<openrider::sdk::SeriesSpec> graphs = {openrider::sdk::SeriesSpec{}};  ///< stacked graphs; starts with one
    int hover_index = -1;          ///< point under the mouse in a graph (a bar's first), -1 = none
    int hover_end_index = -1;      ///< a hovered histogram bar's last point, -1 = a single point
    double hover_x = 0.0;          ///< its x value (cursor line; a bar's centre)
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
