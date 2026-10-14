#include <openrider/sdk/route_ranges.h>

#include <openrider/sdk/local_projection.h>
#include <openrider/sdk/plane_geometry.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <string>

namespace openrider::sdk {

namespace {

/// Weight of a cut's distance to its node against the points' distances to
/// their segments: a cut is drawn to the node, as when the track turns
/// around there.
constexpr double kNodeWeight = 2.0;

/// Beyond this distance (m) from a segment's bounding box, a point's
/// distance to the segment is taken as its distance to the box: such a
/// point is not on that segment, and the exact value would cost a walk
/// along the whole line.
constexpr double kNearBoxM = 100.0;

/// A segment's line in the plane, with its bounding box.
struct PlaneLine {
    std::vector<XY> points;  ///< the line
    double min_x = 0.0;      ///< bounding box
    double min_y = 0.0;      ///< bounding box
    double max_x = 0.0;      ///< bounding box
    double max_y = 0.0;      ///< bounding box
};

PlaneLine ToPlane(const NetworkSegment &segment, const LocalProjection &projection)
{
    PlaneLine line;
    line.min_x = line.min_y = std::numeric_limits<double>::max();
    line.max_x = line.max_y = std::numeric_limits<double>::lowest();
    for (size_t i = 0; i < segment.lat.size(); ++i) {
        const XY q = projection.Forward(segment.lon[i], segment.lat[i]);
        line.points.push_back(q);
        line.min_x = std::min(line.min_x, q.x);
        line.min_y = std::min(line.min_y, q.y);
        line.max_x = std::max(line.max_x, q.x);
        line.max_y = std::max(line.max_y, q.y);
    }
    return line;
}

/// Distance from `q` to `line`: exact near it, to its bounding box beyond.
double DistanceTo(const XY &q, const PlaneLine &line)
{
    if (line.points.empty())
        return 0.0;
    const double dx = std::max({line.min_x - q.x, 0.0, q.x - line.max_x});
    const double dy = std::max({line.min_y - q.y, 0.0, q.y - line.max_y});
    const double to_box = std::hypot(dx, dy);
    if (to_box > kNearBoxM)
        return to_box;
    return line.points.size() == 1 ? Dist(q, line.points[0]) : DistanceToLine(q, line.points);
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
