/// @file
/// A track as segment analysis sees it: its moving recorded points in the
/// local plane.
// @impl 0f87-6acf-6d54-b5b0
#pragma once

#include <openrider/sdk/local_projection.h>
#include <openrider/sdk/plane_geometry.h>
#include <openrider/sdk/recorded_track.h>
#include <openrider/sdk/rests.h>

#include <cstddef>
#include <vector>

namespace openrider::sdk {

/// The recorded points of a track that have a position and are moving (the
/// not-moving rule: a stop's jitter says nothing of the way), in the plane,
/// with their velocity (PointVelocities()) and the path walked along them.
/// What segment methods analyse: recorded points as they are, never
/// resampled or thinned.
struct PlaneTrack {
    std::vector<XY> points;      ///< positions
    std::vector<XY> velocities;  ///< velocities (m/s), from the recorded speed
    std::vector<double> along;   ///< path from the first point (m), along the kept positions
    std::vector<size_t> breaks;  ///< indices k where a rest lies between points k - 1 and k

    /// Number of points.
    size_t size() const { return points.size(); }

    /// Points first..last (inclusive), their path still counted from this
    /// track's first point.
    PlaneTrack Slice(size_t first, size_t last) const;
};

/// `track`'s moving points with a position, in `projection`'s plane, but
/// for the points of `rests` (a pause is left out whole, walks around it
/// included); each rest breaks the track there.
PlaneTrack ToPlane(const RecordedTrack &track, const LocalProjection &projection, const std::vector<Rest> &rests = {});

} // namespace openrider::sdk
