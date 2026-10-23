#include "app/map_view_utils.h"


#include <algorithm>
#include <cmath>

namespace app {

const TrackStyleOption kTrackStyles[6] = {
    {"Solid colour", openrider::sdk::SeriesKind::Altitude},  // kind unused for solid
    {"Speed", openrider::sdk::SeriesKind::Speed},
    {"Heart rate", openrider::sdk::SeriesKind::HeartRate},
    {"Altitude", openrider::sdk::SeriesKind::Altitude},
    {"Grade", openrider::sdk::SeriesKind::Grade},
    {"Vertical speed", openrider::sdk::SeriesKind::VerticalSpeed},
};

int MaxViewZoom(const MapLayerInfo *layer)
{
    return layer ? std::max(kMaxViewZoom, layer->max_zoom) : kMaxViewZoom;
}

maptiles::GeoBox MapViewBox(const MapViewModel &view_model)
{
    if (!view_model.Layer() || view_model.width <= 0.0f)
        return {-180, -85, 180, 85};
    return MapGeometry(view_model).ViewBox();
}

// @impl 0f87-6ada-53d7-171a
int SourceGrid(const MapViewModel &view_model, const MapGeometry &geometry)
{
    const MapLayerInfo *layer = view_model.Layer();
    if (!layer || layer->grids.empty())
        return -1;
    const int display = geometry.Display().epsg;
    const MapCamera &camera = view_model.Camera();
    auto usable = [&](size_t g) { return layer->grids[g].crs.Valid() && !layer->grids[g].tiles.levels.empty(); };
    if (display != 3857)
        for (size_t g = 0; g < layer->grids.size(); ++g)
            if (usable(g) && layer->grids[g].crs.epsg == display)
                return static_cast<int>(g);
    // A local grid: its system's area of use holds the centre, else within
    // a margin of 3 degrees.
    for (double margin : {0.0, 3.0})
        for (size_t g = 0; g < layer->grids.size(); ++g) {
            const reproj::Crs &crs = layer->grids[g].crs;
            if (usable(g) && crs.epsg != 3857 && !crs.geographic && camera.lon >= crs.area.x0 - margin &&
                camera.lon <= crs.area.x1 + margin && camera.lat >= crs.area.y0 - margin && camera.lat <= crs.area.y1 + margin)
                return static_cast<int>(g);
        }
    for (size_t g = 0; g < layer->grids.size(); ++g)
        if (usable(g) && layer->grids[g].crs.epsg == 3857)
            return static_cast<int>(g);
    for (size_t g = 0; g < layer->grids.size(); ++g)
        if (usable(g))
            return static_cast<int>(g);
    return -1;
}

std::vector<VisibleTile> VisibleTiles(const MapViewModel &view_model, const MapGeometry &geometry, int grid)
{
    std::vector<VisibleTile> visible;
    const MapLayerInfo *layer = view_model.Layer();
    if (!layer || grid < 0 || grid >= static_cast<int>(layer->grids.size()) || !geometry.Valid())
        return visible;
    const MapGrid &source = layer->grids[static_cast<size_t>(grid)];
    const reproj::Transformer to_source(geometry.Display(), source.crs);
    // The view in the grid's system: its border and centre, transformed.
    double x0 = 1e300, y0 = 1e300, x1 = -1e300, y1 = -1e300;
    constexpr int kSteps = 8;
    for (int i = 0; i <= kSteps; ++i)
        for (int j = 0; j <= kSteps; ++j) {
            if (i != 0 && i != kSteps && j != 0 && j != kSteps && !(i == kSteps / 2 && j == kSteps / 2))
                continue;
            double dx, dy, sx, sy;
            geometry.ScreenToDisplay(geometry.Width() * i / kSteps, geometry.Height() * j / kSteps, dx, dy);
            if (!to_source.Transform(dx, dy, sx, sy))
                continue;
            x0 = std::min(x0, sx);
            x1 = std::max(x1, sx);
            y0 = std::min(y0, sy);
            y1 = std::max(y1, sy);
        }
    if (x0 > x1)
        return visible;
    // The size of a screen pixel in the grid's system, at the centre.
    double ax, ay, bx, by, sax, say, sbx, sby;
    geometry.ScreenToDisplay(geometry.Width() / 2.0 - 0.5, geometry.Height() / 2.0 - 0.5, ax, ay);
    geometry.ScreenToDisplay(geometry.Width() / 2.0 + 0.5, geometry.Height() / 2.0 + 0.5, bx, by);
    if (!to_source.Transform(ax, ay, sax, say) || !to_source.Transform(bx, by, sbx, sby))
        return visible;
    const double pixel = std::hypot(sbx - sax, sby - say) / std::sqrt(2.0);
    // The level whose pixels best match; the finest past the last one.
    const std::vector<maptiles::TileMatrix> &levels = source.tiles.levels;
    const int first = std::clamp(source.tiles.first_usable, 0, static_cast<int>(levels.size()) - 1);
    int level = first;
    for (int l = first; l < static_cast<int>(levels.size()); ++l)
        if (std::fabs(std::log2(levels[static_cast<size_t>(l)].resolution / pixel)) <
            std::fabs(std::log2(levels[static_cast<size_t>(level)].resolution / pixel)))
            level = l;
    // Web Mercator shown in Web Mercator wraps around the antimeridian.
    const bool wraps = source.crs.epsg == 3857 && geometry.Display().epsg == 3857;
    for (; level >= first; --level) {
        const maptiles::TileMatrix &m = levels[static_cast<size_t>(level)];
        const double tile_w = m.resolution * m.tile_width, tile_h = m.resolution * m.tile_height;
        int first_column = static_cast<int>(std::floor((x0 - m.origin_x) / tile_w));
        int last_column = static_cast<int>(std::floor((x1 - m.origin_x) / tile_w));
        int first_row = static_cast<int>(std::floor((m.origin_y - y1) / tile_h));
        int last_row = static_cast<int>(std::floor((m.origin_y - y0) / tile_h));
        first_row = std::max(first_row, m.first_row);
        last_row = std::min(last_row, m.last_row);
        if (!wraps) {
            first_column = std::max(first_column, m.first_column);
            last_column = std::min(last_column, m.last_column);
        }
        if (first_row > last_row || first_column > last_column)
            return visible;
        const size_t count = static_cast<size_t>(last_row - first_row + 1) * static_cast<size_t>(last_column - first_column + 1);
        if (count > kMaxVisibleTiles && level > first)
            continue;  // too many: one level coarser
        for (int row = first_row; row <= last_row; ++row)
            for (int column = first_column; column <= last_column; ++column) {
                VisibleTile tile;
                tile.key.grid = grid;
                tile.key.z = level;
                tile.key.x = wraps ? ((column % m.matrix_width) + m.matrix_width) % m.matrix_width : column;
                if (tile.key.x < m.first_column || tile.key.x > m.last_column)
                    continue;  // wrapped beyond the layer's limits
                tile.key.y = row;
                tile.extent = {m.origin_x + column * tile_w, m.origin_y - row * tile_h, m.origin_x + (column + 1) * tile_w,
                               m.origin_y - (row + 1) * tile_h};
                if (visible.size() < kMaxVisibleTiles)
                    visible.push_back(tile);
            }
        break;
    }
    return visible;
}

std::vector<int> MapOfferedLayers(const MapViewModel &view_model, const AppModel &model)
{
    std::vector<int> offered;
    const maptiles::GeoBox view = MapViewBox(view_model);
    std::vector<double> lons, lats;
    if (view_model.mode == MapMode::Outing)
        for (const or_point &point : model.points) {
            lons.push_back(point.lon);
            lats.push_back(point.lat);
        }
    for (int i = 0; i < static_cast<int>(view_model.layers.size()); ++i)
        if (i == view_model.current ||
            maptiles::LayerOffered(view_model.layers[static_cast<size_t>(i)].extent, view, lons, lats))
            offered.push_back(i);
    return offered;
}

} // namespace app
