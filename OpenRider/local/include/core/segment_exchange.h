/// @file
/// Exchange of segment networks with segments plugins (module-level
/// utilities): the application's tracks to the plugin ABI's, the plugin's
/// network back to core::SegmentNetwork, and the reverse for plugins
/// written in C++ on top of libcore.
#pragma once

#include "core/overview.h"
#include "core/result.h"
#include "core/segments.h"

#include <openrider/segments.h>

#include <vector>

namespace core {

/// Runs a segments plugin on `tracks`.
/// @param plugin the plugin's vtable
/// @param tracks the tracks of one group of similar activities
/// @param values one value per declared parameter
/// @param cancel polled by the plugin; non-zero stops it (may be null)
/// @param user passed to `cancel`
/// @return the network (route ids from the tracks' ids), or the plugin's error
Result<SegmentNetwork> RunSegments(const or_segments_vtable *plugin, const std::vector<OverviewTrack> &tracks,
                                   const std::vector<double> &values, or_segment_cancel_cb cancel, void *user);

/// The network a plugin returned, as core types; routes named after
/// `tracks` (the plugin's input, in order).
SegmentNetwork ImportNetwork(const or_segment_network &network, const std::vector<OverviewTrack> &tracks);

/// `network` in the plugin ABI's form, allocated with malloc; route ids
/// become indices into `tracks`. Release with FreeExportedNetwork().
or_segment_network *ExportNetwork(const SegmentNetwork &network, const std::vector<OverviewTrack> &tracks);

/// Releases a network made by ExportNetwork().
void FreeExportedNetwork(or_segment_network *network);

/// The plugin ABI's tracks as core's: positions and noise copied, bounding
/// boxes computed.
std::vector<OverviewTrack> ImportTracks(const or_segment_track *tracks, size_t count);

} // namespace core
