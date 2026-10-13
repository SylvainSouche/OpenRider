/** @file
 * OpenRider plugin ABI.
 *
 * A plugin is a shared library placed in `plugins/<kind>/`. It exports one
 * extern "C" function, named by OR_PLUGIN_ENTRY_SYMBOL, returning a static
 * descriptor. The application refuses a plugin whose api_version differs
 * from the one it was built with. Everything crossing this boundary is
 * plain C: no C++ types, no exceptions, no RTTI.
 */
#ifndef OPENRIDER_PLUGIN_H
#define OPENRIDER_PLUGIN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* @impl 0f87-6acd-24ce-dfb9 */
/** Version of this ABI; a plugin built for another one is refused. */
#define OR_PLUGIN_API_VERSION 1u
/** Name of the function every plugin exports (an or_plugin_entry_fn). */
#define OR_PLUGIN_ENTRY_SYMBOL "openrider_plugin"

/** Plugin kinds; each one has its own subdirectory under plugins/. */
enum or_plugin_kind {
    OR_PLUGIN_INPUT = 1,    /**< plugins/input: data backends, see input.h */
    OR_PLUGIN_FILTER = 2,   /**< plugins/filters: see filter.h */
    OR_PLUGIN_TILES = 3,    /**< plugins/tiles: map tile protocols, see tiles.h */
    OR_PLUGIN_WORKFLOW = 4  /**< plugins/workflows: external tools */
};

/** The descriptor a plugin returns from its entry function. */
struct or_plugin_info {
    uint32_t api_version;   /**< must be OR_PLUGIN_API_VERSION */
    uint32_t kind;          /**< enum or_plugin_kind */
    const char *name;       /**< short identifier, e.g. "garmindb" */
    const char *version;    /**< the plugin's own version string */
    const void *vtable;     /**< kind-specific function table, e.g. or_input_vtable */
};

/** Type of the entry function named by OR_PLUGIN_ENTRY_SYMBOL. */
typedef const struct or_plugin_info *(*or_plugin_entry_fn)(void);

#ifdef __cplusplus
}
#endif

#endif /* OPENRIDER_PLUGIN_H */
