/// @file
/// The document a layer needs before describing its grids (WMTS
/// capabilities): downloaded once a week at most, kept in the tile cache,
/// and the cached copy used while the service is unreachable
/// (module-level utilities).
#pragma once

#include <openrider/tiles.h>

#include <string>

namespace maptiles {

/// Capabilities older than this are downloaded again (s): a week.
constexpr double kCapabilitiesMaxAge = 7 * 24 * 3600.0;

/// Gives an open layer the document its plugin asks for, if any: the
/// cached copy `<cache_dir>/<layer id>/capabilities.xml` when recent,
/// else a download (the layer's headers, `user_agent`), cached; a stale
/// copy when the download fails.
/// @return "" when the layer has what it needs, else why not
std::string ProvideCapabilities(const or_tiles_vtable *tiles_vtable, void *layer, const std::string &cache_dir,
                                const std::string &user_agent);

} // namespace maptiles
