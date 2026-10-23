#include "maptiles/tile_grid.h"

#include "core/plugin_thread.h"

#include <cstddef>

namespace maptiles {

namespace {
// Half the width of the Web Mercator world, metres.
constexpr double kHalfWorld = 20037508.342789244;
} // namespace

bool HasGridFunctions(const or_tiles_vtable *tiles_vtable)
{
    return tiles_vtable->struct_size >= offsetof(or_tiles_vtable, grid_tile_url) + sizeof(tiles_vtable->grid_tile_url) &&
           tiles_vtable->grid_count && tiles_vtable->grid && tiles_vtable->grid_tile_url;
}

TileGrid WebMercatorGrid(int min_zoom, int max_zoom, int tile_size)
{
    TileGrid grid;
    grid.crs = "EPSG:3857";
    grid.xyz = true;
    grid.first_usable = min_zoom;
    for (int z = 0; z <= max_zoom; ++z) {
        TileMatrix level;
        level.id = std::to_string(z);
        level.matrix_width = level.matrix_height = 1 << z;
        level.resolution = 2.0 * kHalfWorld / (static_cast<double>(tile_size) * level.matrix_width);
        level.origin_x = -kHalfWorld;
        level.origin_y = kHalfWorld;
        level.tile_width = level.tile_height = tile_size;
        level.last_column = level.last_row = level.matrix_width - 1;
        grid.levels.push_back(level);
    }
    return grid;
}

std::vector<TileGrid> GridsOf(const or_tiles_vtable *tiles_vtable, void *layer)
{
    std::vector<TileGrid> grids;
    core::CallPlugin(tiles_vtable, [&] {
        if (HasGridFunctions(tiles_vtable))
            for (size_t g = 0, count = tiles_vtable->grid_count(layer); g < count; ++g) {
                const or_tile_grid *described = tiles_vtable->grid(layer, g);
                if (!described || !described->crs || described->level_count == 0)
                    continue;
                TileGrid grid;
                grid.id = described->id ? described->id : std::to_string(g);
                grid.crs = described->crs;
                for (size_t l = 0; l < described->level_count; ++l) {
                    const or_tile_matrix &m = described->levels[l];
                    if (!(m.resolution > 0.0) || m.tile_width <= 0 || m.tile_height <= 0)
                        continue;
                    grid.levels.push_back({m.id ? m.id : std::to_string(l), m.resolution, m.origin_x, m.origin_y,
                                           m.tile_width, m.tile_height, m.matrix_width, m.matrix_height,
                                           m.first_column, m.last_column, m.first_row, m.last_row});
                }
                if (!grid.levels.empty())
                    grids.push_back(std::move(grid));
            }
        if (grids.empty()) {
            const or_tile_layer *info = tiles_vtable->layer(layer);
            grids.push_back(WebMercatorGrid(info->min_zoom, info->max_zoom, info->tile_size));
        }
    });
    return grids;
}

std::string CacheName(const std::string &layer_id, const TileGrid &grid)
{
    return grid.xyz || grid.id.empty() ? layer_id : layer_id + "@" + grid.id;
}

} // namespace maptiles
