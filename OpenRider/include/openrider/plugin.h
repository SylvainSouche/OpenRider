/** @file
 * OpenRider plugin ABI.
 *
 * A plugin is a shared library placed in `plugins/<kind>/`. It exports one
 * extern "C" function, named by OR_PLUGIN_ENTRY_SYMBOL, returning a static
 * descriptor. Everything crossing this boundary is plain C: no C++ types,
 * no exceptions, no RTTI.
 *
 * Versions: the application refuses a plugin whose api_version (major)
 * differs from its own, or whose api_minor is newer than its own. Minor
 * versions only append: fields at the end of the descriptor, functions at
 * the end of a kind's function table. Every function table starts with
 * its struct_size, so that either side can tell which functions the other
 * provides.
 *
 * Threads: a plugin declaring OR_PLUGIN_THREAD_SAFE may be called from any
 * thread, concurrently, on the same handle. Without it, the application
 * makes every call to the plugin from one thread of its own.
 */
#ifndef OPENRIDER_PLUGIN_H
#define OPENRIDER_PLUGIN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* @impl 0f87-6acd-24ce-dfb9 */
/** Major version of this ABI; a plugin built for another one is refused. */
#define OR_PLUGIN_API_VERSION 2u
/** Minor version of this ABI; a plugin built for a newer one is refused. */
#define OR_PLUGIN_API_MINOR 0u
/* @impl 0f87-6ace-72ef-5cb5 */
/** Name of the function every plugin exports (an or_plugin_entry_fn). */
#define OR_PLUGIN_ENTRY_SYMBOL "openrider_plugin"

/** Plugin kinds; each one has its own subdirectory under plugins/. */
enum or_plugin_kind {
    OR_PLUGIN_INPUT = 1,    /**< plugins/input: data backends, see input.h */
    OR_PLUGIN_FILTER = 2,   /**< plugins/filters: see filter.h */
    OR_PLUGIN_TILES = 3,    /**< plugins/tiles: map tile protocols, see tiles.h */
    OR_PLUGIN_WORKFLOW = 4, /**< plugins/workflows: external tools */
    OR_PLUGIN_SEGMENTS = 5  /**< plugins/segments: segment identification, see segments.h */
};

/** Flags a plugin declares in its descriptor. */
enum or_plugin_flags {
    /** Its functions may be called from several threads at once, on the
     * same handle. */
    OR_PLUGIN_THREAD_SAFE = 1u << 0
};

/** The descriptor a plugin returns from its entry function. */
struct or_plugin_info {
    uint32_t api_version;   /**< OR_PLUGIN_API_VERSION the plugin was built with */
    uint32_t api_minor;     /**< OR_PLUGIN_API_MINOR the plugin was built with */
    uint32_t kind;          /**< enum or_plugin_kind */
    uint32_t flags;         /**< enum or_plugin_flags, or-ed */
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
