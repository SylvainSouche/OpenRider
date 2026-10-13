/// @file
/// Tuning constants of the track matching, shared by its stages.
#pragma once

#include <limits>

namespace matching {

/// An impossible transition.
inline constexpr double kInfinity = std::numeric_limits<double>::infinity();

/// @name Costs of the matching, in units of a point at the tolerance distance
/// @{
inline constexpr double kOffCost = 1.0;     ///< a point off the network
inline constexpr double kSwitchCost = 2.0;  ///< entering or leaving the network
inline constexpr double kNodeCost = 0.5;    ///< passing from a segment to the next
inline constexpr double kTurnCost = 3.0;    ///< turning back on a segment
/// @}

/// "At the end" of a segment: within this many stations.
inline constexpr int kEndStations = 3;

/// Final lines smoothed over +/- this many stations.
inline constexpr int kSmoothStations = 4;

} // namespace matching
