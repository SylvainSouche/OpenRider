#include "app/comparison_view_controller.h"

namespace app {

void ComparisonViewController::Tick(const AppModel &model)
{
    if (model.comparison.generation == generation_ && view_model_.shown.size() == model.comparison.runs.size())
        return;
    generation_ = model.comparison.generation;
    view_model_.shown.assign(model.comparison.runs.size(), true);
    view_model_.hovering = false;
}

void ComparisonViewController::Handle(const Intent &intent)
{
    const int graph_count = static_cast<int>(view_model_.graphs.size());
    switch (intent.kind) {
    case IntentKind::ToggleComparedRun:
        if (intent.i >= 0 && intent.i < static_cast<int>(view_model_.shown.size()))
            view_model_.shown[static_cast<size_t>(intent.i)] = !view_model_.shown[static_cast<size_t>(intent.i)];
        break;
    case IntentKind::ShowAllComparedRuns:
        view_model_.shown.assign(view_model_.shown.size(), intent.i != 0);
        break;
    case IntentKind::SetComparisonXAxis:
        view_model_.xaxis = static_cast<openrider::sdk::XAxis>(intent.i);
        view_model_.hovering = false;
        break;
    case IntentKind::ComparisonHover:
        view_model_.hovering = true;
        view_model_.hover_x = intent.x;
        break;
    case IntentKind::ComparisonHoverEnd:
        view_model_.hovering = false;
        break;
    case IntentKind::AddComparisonGraph:
        view_model_.graphs.push_back(openrider::sdk::SeriesSpec{});
        break;
    case IntentKind::RemoveComparisonGraph:
        if (graph_count > 1 && intent.i >= 0 && intent.i < graph_count)
            view_model_.graphs.erase(view_model_.graphs.begin() + intent.i);
        break;
    case IntentKind::SetComparisonGraphSpec:
        if (intent.i >= 0 && intent.i < graph_count)
            view_model_.graphs[static_cast<size_t>(intent.i)] = intent.spec;
        break;
    default:
        break;
    }
}

} // namespace app
