/// @file
/// The main window's layout: sizes of the resizable areas and the About box.
#pragma once

#include "app/intents.h"

namespace app {

/// What the main view keeps of its layout.
struct MainViewModel {
    float list_width = 380.0f;  ///< outing list, pixels
    float map_height = 0.0f;    ///< outing map, pixels; 0 = 45% of the panel
    bool show_about = false;    ///< the About box is open
};

/// Owns the MainViewModel and changes it in response to layout intents.
class MainViewController {
public:
    /// The view model, for the main view.
    const MainViewModel &ViewModel() const { return view_model_; }

    /// Handles ResizeList, ResizeMap and ShowAbout.
    void Handle(const Intent &intent);

private:
    MainViewModel view_model_;  ///< changed only here
};

} // namespace app
