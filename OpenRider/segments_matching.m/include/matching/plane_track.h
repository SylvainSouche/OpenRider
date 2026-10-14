/// @file
/// A track as the matching sees it: its moving recorded points in the
/// local plane.
#pragma once

#include <openrider/sdk/local_projection.h>
#include <openrider/sdk/plane_geometry.h>
#include <openrider/sdk/recorded_track.h>

#include <cstddef>
#include <vector>

namespace matching {

/// The recorded points of a track that have a position and are moving (the
/// SDK's not-moving rule: a stop's jitter says nothing of the way), in the
/// plane, with their velocity and the path walked along them.
struct PlaneTrack {
    std::vector<openrider::sdk::XY> points;      ///< positions
    std::vector<openrider::sdk::XY> velocities;  ///< velocities (m/s), from the recorded speed
    std::vector<double> along;                   ///< path from the first point (m)

    /// Number of points.
    size_t size() const { return points.size(); }

    /// Points first..last (inclusive), their path still counted from this
    /// track's first point.
    PlaneTrack Slice(size_t first, size_t last) const;
};

/// `track`'s moving points with a position, in `projection`'s plane.
PlaneTrack ToPlane(const openrider::sdk::RecordedTrack &track, const openrider::sdk::LocalProjection &projection);

} // namespace matching
