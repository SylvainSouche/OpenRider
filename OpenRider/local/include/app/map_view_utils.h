/// @file
/// Map geometry and options shared by the map view and its controller
/// (module-level utilities).
#pragma once

#include "app/app_model.h"
#include "app/map_geometry.h"
#include "app/map_view_controller.h"

#include "maptiles/coverage.h"

#include <openrider/sdk/series.h>

#include <vector>

namespace app {

/// Highest view zoom for `layer`: at least kMaxViewZoom (over-zoom).
int MaxViewZoom(const MapLayerInfo *layer);

/// Region shown, WGS84; the whole world when no layer is shown.
maptiles::GeoBox MapViewBox(const MapViewModel &view_model);

/// The layer grid the tiles are taken from, -1 if none: in a display
/// other than Web Mercator, the grid native to it; otherwise -- Web
/// Mercator, or a display the layer lacks -- the provider's local grid
/// whose system's area of use holds the view centre (within 3 degrees),
/// warped; else its Web Mercator grid, else its first.
int SourceGrid(const MapViewModel &view_model, const MapGeometry &geometry);

/// A tile to show, and its extent in its grid's system (top-left (x0, y0),
/// bottom-right (x1, y1)).
struct VisibleTile {
    maptiles::TileKey key;  ///< grid, level, column (wrapped around the world), row
    reproj::Box extent;     ///< where it lies, in the grid's system
};

/// The tiles of grid `grid` covering the view, from the level whose pixels
/// best match the screen's (the finest past its last level: over-zoom), at
/// most kMaxVisibleTiles of them (a coarser level otherwise).
std::vector<VisibleTile> VisibleTiles(const MapViewModel &view_model, const MapGeometry &geometry, int grid);

/// Most tiles shown at once.
constexpr size_t kMaxVisibleTiles = 300;

/// Layers worth offering: those covering the view and the selected track,
/// plus the current one.
/// @return indices into view_model.layers
std::vector<int> MapOfferedLayers(const MapViewModel &view_model, const AppModel &model);

/// A way of colouring the outing track.
struct TrackStyleOption {
    const char *name;       ///< display name
    openrider::sdk::SeriesKind kind;  ///< series the colour follows (unused for solid)
};

/// The track styles offered, index 0 being a solid colour.
extern const TrackStyleOption kTrackStyles[6];

} // namespace app
