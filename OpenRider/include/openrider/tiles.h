/** @file
 * OpenRider tile service abstraction.
 *
 * A tiles plugin implements one protocol (XYZ, WMTS, WMS...). Layers are
 * data: each is a JSON definition the plugin parses in open(). The
 * application handles fetching, politeness, caching and display; the plugin
 * only describes the layer and builds tile URLs.
 *
 * Terms: a *service* is a tile provider (e.g. OpenTopoMap); a *server* is
 * one of its interchangeable hosts (e.g. a.tile.opentopomap.org).
 *
 * Since ABI 2.1 a layer may be published in several tile grids (WMTS tile
 * matrix sets), each in its own coordinate system: Web Mercator and the
 * provider's national grid, for instance. A plugin whose layer needs a
 * document from the service to know them (WMTS capabilities) names it in
 * capabilities_url(); the application fetches it (politely, cached) and
 * hands it to set_capabilities(). The grids are then described by
 * grid_count() and grid(), and their tiles addressed by grid_tile_url().
 * A plugin without these functions has one grid: Web Mercator levels
 * min_zoom..max_zoom of tile_size pixels, addressed by tile_url().
 */
#ifndef OPENRIDER_TILES_H
#define OPENRIDER_TILES_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* @impl 0f87-6acd-2f33-62f7 */
/** A map layer as described by its plugin. */
struct or_tile_layer {
    const char *id;           /**< stable identifier, used for the cache path */
    const char *name;         /**< display name */
    const char *crs;          /**< e.g. "EPSG:3857" */
    int min_zoom;             /**< first zoom level served */
    int max_zoom;             /**< last zoom level served */
    int tile_size;            /**< tile width and height, pixels */
    double extent[4];         /**< west, south, east, north (WGS84 degrees) */
    const char *attribution;  /**< shown on the map whenever the layer is */
    size_t server_count;      /**< interchangeable servers (mirrors), >= 1 */
    const char *headers;      /**< extra HTTP headers, "Name: value" lines; "" if none */
};

/* @impl 0f87-6ada-53d7-4404 */
/** One level of a tile grid (a WMTS tile matrix). Coordinates are in the
 * grid's coordinate system, x east and y north, in its units (metres, or
 * degrees for a geographic one). */
struct or_tile_matrix {
    const char *id;           /**< level identifier used in tile URLs */
    double resolution;        /**< size of a pixel, in the system's units */
    double origin_x;          /**< west edge of column 0 */
    double origin_y;          /**< north edge of row 0 */
    int tile_width;           /**< pixels */
    int tile_height;          /**< pixels */
    int matrix_width;         /**< columns */
    int matrix_height;        /**< rows */
    int first_column;         /**< first column holding tiles (the layer's limits) */
    int last_column;          /**< last column holding tiles */
    int first_row;            /**< first row holding tiles */
    int last_row;             /**< last row holding tiles */
};

/** A tile grid a layer is published in (a WMTS tile matrix set). */
struct or_tile_grid {
    const char *id;           /**< e.g. "utm33n"; used for the cache path */
    const char *crs;          /**< "EPSG:<code>" */
    size_t level_count;       /**< levels, coarsest first */
    const struct or_tile_matrix *levels;  /**< level_count levels */
};

/** Functions of a tiles plugin. They return 0 on success; on failure,
 * non-zero with a message written to err (errlen bytes, always
 * NUL-terminated). */
struct or_tiles_vtable {
    size_t struct_size;       /**< sizeof(struct or_tiles_vtable) as built */
    const char *protocol;     /**< e.g. "xyz"; matched against a layer's "protocol" */
    /** Opens a layer from its JSON definition; NULL if this protocol does
     * not accept it. */
    void *(*open)(const char *layer_json, char *err, size_t errlen);
    /** Closes a handle returned by open(). */
    void (*close)(void *handle);
    /** The layer's description, valid until close(). */
    const struct or_tile_layer *(*layer)(void *handle);
    /** Writes into `url` (`len` bytes) the URL of tile (z, x, y) on server
     * `server` (0 <= server < server_count). */
    int (*tile_url)(void *handle, size_t server, int z, int x, int y,
                    char *url, size_t len);

    /* ABI 2.1: tile grids. */

    /** URL of the document the layer needs before describing its grids
     * (WMTS capabilities), fetched with the layer's headers; NULL or ""
     * if none. */
    const char *(*capabilities_url)(void *handle);
    /** Hands over that document (`len` bytes). */
    int (*set_capabilities)(void *handle, const char *document, size_t len,
                            char *err, size_t errlen);
    /** Number of grids the layer is published in; 0 until
     * set_capabilities() succeeded when a document is needed. */
    size_t (*grid_count)(void *handle);
    /** Grid `index`, valid until close() or the next set_capabilities(). */
    const struct or_tile_grid *(*grid)(void *handle, size_t index);
    /** Writes into `url` (`len` bytes) the URL of the tile at `column`,
     * `row` of level `level` (an index into the grid's levels) of grid
     * `grid`, on server `server`. */
    int (*grid_tile_url)(void *handle, size_t server, size_t grid, size_t level,
                         int column, int row, char *url, size_t len);
};

#ifdef __cplusplus
}
#endif

#endif /* OPENRIDER_TILES_H */
