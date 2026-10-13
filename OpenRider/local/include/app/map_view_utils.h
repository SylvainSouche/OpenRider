/// @file
/// Map geometry and options shared by the map view and its controller
/// (module-level utilities).
#pragma once

#include "app/app_model.h"
#include "app/map_view_controller.h"

#include "core/series.h"
#include "maptiles/coverage.h"

#include <vector>

namespace app {

/// World-pixel origin (top-left of the viewport) at the camera's zoom.
void MapOrigin(const MapViewModel &view_model, double &left, double &top);

/// Region shown, WGS84; the whole world when no layer is shown.
maptiles::GeoBox MapViewBox(const MapViewModel &view_model);

/// Layers worth offering: those covering the view and the selected track,
/// plus the current one.
/// @return indices into view_model.layers
std::vector<int> MapOfferedLayers(const MapViewModel &view_model, const AppModel &model);

/// A way of colouring the outing track.
struct TrackStyleOption {
    const char *name;       ///< display name
    core::SeriesKind kind;  ///< series the colour follows (unused for solid)
};

/// The track styles offered, index 0 being a solid colour.
extern const TrackStyleOption kTrackStyles[6];

} // namespace app
