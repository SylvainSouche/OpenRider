#include "app/outing_view_controller.h"

#include "app/graphs.h"

#include <cmath>
#include <limits>

namespace app {

void OutingViewController::Tick(const AppModel &model)
{
    if (model.points_id != points_id_) {
        points_id_ = model.points_id;
        view_model_.hover_index = -1;
        view_model_.hover_end_index = -1;
    }
}

// @impl 0f87-6acd-32ed-0184
void OutingViewController::Handle(const Intent &intent, const AppModel &model)
{
    const int graph_count = static_cast<int>(view_model_.graphs.size());
    switch (intent.kind) {
    case IntentKind::SetXAxis:
        view_model_.xaxis = static_cast<openrider::sdk::XAxis>(intent.i);
        view_model_.hover_index = -1;
        view_model_.hover_end_index = -1;
        break;
    case IntentKind::AddGraph:
        view_model_.graphs.push_back(openrider::sdk::SeriesSpec{});
        break;
    case IntentKind::RemoveGraph:
        if (graph_count > 1 && intent.i >= 0 && intent.i < graph_count)
            view_model_.graphs.erase(view_model_.graphs.begin() + intent.i);
        break;
    case IntentKind::SetGraphSpec:
        if (intent.i >= 0 && intent.i < graph_count)
            view_model_.graphs[static_cast<size_t>(intent.i)] = intent.spec;
        break;
    // @impl 0f87-6ad9-af16-ead5
    case IntentKind::GraphHover: {
        if (intent.i < 0 || intent.i >= graph_count || model.points.empty())
            break;
        // Find the sample under the mouse in the hovered graph, then publish
        // its source point so that the map and the other graphs show it too.
        const openrider::sdk::SeriesSpec &spec = view_model_.graphs[static_cast<size_t>(intent.i)];
        const openrider::sdk::Series series = GraphSeries(model.points, view_model_.xaxis, spec);
        if (series.x.empty())
            break;
        size_t hovered = 0;
        view_model_.hover_end_index = -1;
        if (spec.type == openrider::sdk::GraphType::Histogram) {
            // The bar under the mouse: the last edge at or left of it. The
            // cursor sits at the bar's centre, and the bar's points (from
            // its first to the next bar's first) make the hovered section.
            for (size_t k = 0; k + 1 < series.x.size(); ++k)
                if (series.x[k] <= intent.x)
                    hovered = k;
            if (hovered + 1 < series.x.size()) {
                view_model_.hover_x = (series.x[hovered] + series.x[hovered + 1]) / 2.0;
                view_model_.hover_index = static_cast<int>(series.point_index[hovered]);
                view_model_.hover_end_index = static_cast<int>(series.point_index[hovered + 1]);
                break;
            }
        } else {
            // The sample nearest the mouse along the x axis.
            double nearest = std::numeric_limits<double>::infinity();
            for (size_t k = 0; k < series.x.size(); ++k)
                if (std::fabs(series.x[k] - intent.x) < nearest) {
                    nearest = std::fabs(series.x[k] - intent.x);
                    hovered = k;
                }
        }
        view_model_.hover_x = series.x[hovered];
        view_model_.hover_index =
            static_cast<int>(hovered < series.point_index.size() ? series.point_index[hovered] : hovered);
        break;
    }
    case IntentKind::GraphHoverEnd:
        view_model_.hover_index = -1;
        view_model_.hover_end_index = -1;
        break;
    case IntentKind::SetStatsComputed:
        view_model_.stats_computed = intent.i != 0;
        break;
    default:
        break;
    }
}

} // namespace app
