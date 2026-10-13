#include "app/main_view_controller.h"

namespace app {

// @impl 0f87-6acd-ce40-cb62
void MainViewController::Handle(const Intent &intent)
{
    switch (intent.kind) {
    case IntentKind::ResizeList:
        view_model_.list_width = static_cast<float>(intent.x);
        break;
    case IntentKind::ResizeMap:
        view_model_.map_height = static_cast<float>(intent.y);
        break;
    case IntentKind::ShowAbout:
        view_model_.show_about = intent.i != 0;
        break;
    default:
        break;
    }
}

} // namespace app
