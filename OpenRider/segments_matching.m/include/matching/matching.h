/// @file
/// Segment identification by matching tracks into a growing network: the
/// segments_matching plugin's algorithm.
///
/// Method (incremental map construction from GPS traces):
///  1. each track's recorded points that are moving (the SDK's not-moving
///     rule) taken as they are, each with its own velocity (recorded speed,
///     direction of travel), and split into passes at its turn-arounds (so
///     that a stretch done out and back matches itself on the way back);
///     analysis works on recorded points, never on resampled or thinned
///     lines whose features may be artefacts;
///  2. passes inserted one after the other. A pass is matched to the
///     network by the cheapest sequence of states (a hidden Markov model,
///     as in map matching): each point is on a segment -- its line within
///     `tolerance_m` beside the point, the point's velocity along it within
///     `max_angle_deg` -- or off the network. Consecutive points must
///     advance along the same segment by the path walked between them, or
///     pass from the end of one segment to the start of the
///     next through their common node. Continuity is what keeps apart two
///     paths that run close together: a switchback's legs, a hairpin's;
///  3. matched points pull the segment's shape towards them (running mean
///     per station, so a segment followed ten times moves a tenth as much
///     as one followed once); a stretch off the network at least
///     `min_new_m` of path long
///     becomes a new segment, cutting the segments where it leaves and
///     rejoins the network (the junctions);
///  4. every track matched again to the finished network: segments are
///     cut where a track turns back mid-segment, then each track gives its
///     chain of segments;
///  5. shared clean-up (core/network_cleanup.h).
/// Rests (the SDK's RestIntervals: a pause at a place, walks around it
/// included) are left out of the matching and break their track into
/// passes; every rest place cuts the segments passing it, and the clean-up
/// keeps those nodes: a place where tracks stop is a segment boundary.
/// Callers group tracks of similar activities before calling.
#pragma once

#include <openrider/sdk/recorded_track.h>
#include <openrider/sdk/rests.h>
#include <openrider/sdk/segments.h>

#include <functional>
#include <vector>

namespace matching {

/// Parameters of the matching.
struct MatchParams {
    double tolerance_m = 15.0;    ///< a point this close to a segment, moving along it, can be on it
    double min_new_m = 40.0;      ///< a stretch off the network at least this long becomes a segment
    double max_angle_deg = 50.0;  ///< direction of travel within this of the segment's
    double station_m = 5.0;       ///< spacing of the stations along segments
    openrider::sdk::RestParams rest;  ///< what makes a rest (left out, a place cut into the segments)
};

/// Builds the segment network of `tracks` (see the file description).
/// `cancelled`, when given, is polled between passes; once it returns true
/// the result is incomplete and must be discarded.
openrider::sdk::SegmentNetwork BuildNetwork(const std::vector<openrider::sdk::RecordedTrack> &tracks, const MatchParams &params,
                                  const std::function<bool()> &cancelled = {});

} // namespace matching
