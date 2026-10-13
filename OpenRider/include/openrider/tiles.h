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

/** Functions of a tiles plugin. They return 0 on success; on failure,
 * non-zero with a message written to err (errlen bytes, always
 * NUL-terminated). */
struct or_tiles_vtable {
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
};

#ifdef __cplusplus
}
#endif

#endif /* OPENRIDER_TILES_H */
