#include <openrider/sdk/route_ranges.h>

#include <openrider/sdk/local_projection.h>
#include <openrider/sdk/plane_geometry.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <unordered_map>

namespace openrider::sdk {

namespace {

/// Weight of a cut's distance to its node against the points' distances to
/// their segments: a cut is drawn to the node, as when the track turns
/// around there.
constexpr double kNodeWeight = 2.0;

/// Cells (m) of the index of a segment's line pieces. A point farther than
/// one cell from every piece is not on that segment: its distance is taken
/// as one cell (the cost of being off it), not walked along the whole line.
constexpr double kPieceCellM = 50.0;

/// Grid key of cell (x, y).
int64_t CellKey(int64_t x, int64_t y) { return (x << 32) ^ (y & 0xffffffffLL); }

/// A segment's line in the plane, its pieces indexed by grid cell.
struct PlaneLine {
    std::vector<XY> points;                                   ///< the line
    std::unordered_map<int64_t, std::vector<size_t>> pieces;  ///< cell -> pieces (i: points i, i + 1) crossing it
};

PlaneLine ToPlane(const NetworkSegment &segment, const LocalProjection &projection)
{
    PlaneLine line;
    for (size_t i = 0; i < segment.lat.size(); ++i)
        line.points.push_back(projection.Forward(segment.lon[i], segment.lat[i]));
    for (size_t i = 0; i + 1 < line.points.size(); ++i) {
        const XY &a = line.points[i], &b = line.points[i + 1];
        const int64_t x0 = static_cast<int64_t>(std::floor(std::min(a.x, b.x) / kPieceCellM));
        const int64_t x1 = static_cast<int64_t>(std::floor(std::max(a.x, b.x) / kPieceCellM));
        const int64_t y0 = static_cast<int64_t>(std::floor(std::min(a.y, b.y) / kPieceCellM));
        const int64_t y1 = static_cast<int64_t>(std::floor(std::max(a.y, b.y) / kPieceCellM));
        for (int64_t x = x0; x <= x1; ++x)
            for (int64_t y = y0; y <= y1; ++y)
                line.pieces[CellKey(x, y)].push_back(i);
    }
    return line;
}

/// Distance from `q` to `line`, capped at kPieceCellM: only the pieces in
/// the cells around the point are measured.
double DistanceTo(const XY &q, const PlaneLine &line)
{
    if (line.points.size() == 1)
        return std::min(kPieceCellM, Dist(q, line.points[0]));
    double best = kPieceCellM;
    const int64_t cx = static_cast<int64_t>(std::floor(q.x / kPieceCellM));
    const int64_t cy = static_cast<int64_t>(std::floor(q.y / kPieceCellM));
    for (int64_t dy = -1; dy <= 1; ++dy)
        for (int64_t dx = -1; dx <= 1; ++dx) {
            const auto found = line.pieces.find(CellKey(cx + dx, cy + dy));
            if (found == line.pieces.end())
                continue;
            for (size_t i : found->second)
                best = std::min(best, DistanceToLine(q, {line.points[i], line.points[i + 1]}));
        }
    return best;
}

/// Cuts of a track of `count` points into `steps` stretches in proportion
/// to the index, when it has fewer points than steps.
std::vector<size_t> EvenCuts(size_t count, size_t steps)
{
    std::vector<size_t> cuts(steps + 1);
    for (size_t k = 0; k <= steps; ++k)
        cuts[k] = count ? k * (count - 1) / steps : 0;
    return cuts;
}

/// Cuts of the track `points` into the steps of a route: cuts[k] is the
/// first point of step k (and the last of step k - 1); cuts[steps] is the
/// last point.
std::vector<size_t> BestCuts(const std::vector<XY> &points, const std::vector<const PlaneLine *> &lines,
                             const std::vector<XY> &nodes)
{
    const size_t count = points.size(), steps = lines.size();
    if (count < steps + 1)
        return EvenCuts(count, steps);
    // cost[k]: least cost of points 0..i with point i on step k; came[i][k]:
    // point i - 1 was on step k - 1 (point i starts step k).
    const double infinite = std::numeric_limits<double>::infinity();
    std::vector<double> cost(steps, infinite), next(steps);
    std::vector<std::vector<bool>> came(count, std::vector<bool>(steps, false));
    cost[0] = DistanceTo(points[0], *lines[0]);
    for (size_t i = 1; i < count; ++i) {
        // Step k is reachable at point i only if steps k + 1.. still have a
        // point each after it.
        const size_t highest = std::min(steps - 1, i);
        const size_t lowest = steps - 1 > count - 1 - i ? steps - 1 - (count - 1 - i) : 0;
        for (size_t k = 0; k < steps; ++k) {
            next[k] = infinite;
            if (k > highest || k < lowest)
                continue;
            const double stay = cost[k];
            const double enter = k > 0 ? cost[k - 1] + kNodeWeight * Dist(points[i], nodes[k]) : infinite;
            came[i][k] = enter < stay;
            next[k] = std::min(stay, enter) + DistanceTo(points[i], *lines[k]);
        }
        cost.swap(next);
    }
    std::vector<size_t> cuts(steps + 1);
    cuts[steps] = count - 1;
    size_t k = steps - 1;
    for (size_t i = count - 1; i > 0 && k > 0; --i)
        if (came[i][k])
            cuts[k--] = i;
    cuts[0] = 0;
    return cuts;
}

} // namespace

void AssignStepRanges(SegmentNetwork &network, const std::vector<RecordedTrack> &tracks)
{
    std::map<std::string, const RecordedTrack *> by_id;
    for (const RecordedTrack &track : tracks)
        by_id[track.id] = &track;
    for (NetworkRoute &route : network.routes) {
        const auto found = by_id.find(route.id);
        if (route.steps.empty() || found == by_id.end() || found->second->points.empty())
            continue;
        const std::vector<or_point> &recorded = found->second->points;
        // The points with a position, and where they are among all.
        std::vector<size_t> located;
        for (size_t i = 0; i < recorded.size(); ++i)
            if (!std::isnan(recorded[i].lat) && !std::isnan(recorded[i].lon))
                located.push_back(i);
        if (located.empty())
            continue;
        const or_point &middle = recorded[located[located.size() / 2]];
        const LocalProjection projection(middle.lon, middle.lat);
        std::vector<XY> points;
        for (size_t i : located)
            points.push_back(projection.Forward(recorded[i].lon, recorded[i].lat));
        // Each segment of the route in the plane, once; the node a step
        // starts from.
        std::map<int, PlaneLine> planes;
        std::vector<const PlaneLine *> lines;
        std::vector<XY> nodes;
        for (const RouteStep &step : route.steps) {
            const NetworkSegment &segment = network.segments[static_cast<size_t>(step.segment)];
            auto plane = planes.find(step.segment);
            if (plane == planes.end())
                plane = planes.emplace(step.segment, ToPlane(segment, projection)).first;
            lines.push_back(&plane->second);
            const NetworkNode &node = network.nodes[static_cast<size_t>(step.forward ? segment.from : segment.to)];
            nodes.push_back(projection.Forward(node.lon, node.lat));
        }
        const std::vector<size_t> cuts = BestCuts(points, lines, nodes);
        for (size_t k = 0; k < route.steps.size(); ++k) {
            route.steps[k].first = k == 0 ? 0 : located[cuts[k]];
            route.steps[k].last = k + 1 == route.steps.size() ? recorded.size() - 1 : located[cuts[k + 1]];
        }
    }
}

} // namespace openrider::sdk
