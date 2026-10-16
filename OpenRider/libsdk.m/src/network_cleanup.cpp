#include <openrider/sdk/network_cleanup.h>

#include <openrider/sdk/motion.h>
#include <openrider/sdk/plane_geometry.h>

#include <algorithm>
#include <cmath>
#include <set>
#include <string>

namespace openrider::sdk {

namespace {

constexpr double kPi = 3.14159265358979323846;

// Removes the segments marked in `drop` from the network: from the routes'
// steps, then renumbering segments and nodes (MergeThroughNodes does the
// renumbering when nothing is left to merge).
void RemoveSegments(SegmentNetwork &net, const std::vector<bool> &drop)
{
    for (NetworkRoute &route : net.routes) {
        std::vector<RouteStep> steps;
        for (const RouteStep &step : route.steps)
            if (!drop[static_cast<size_t>(step.segment)])
                steps.push_back(step);
        route.steps.swap(steps);
    }
    for (NetworkSegment &segment : net.segments)
        if (drop[static_cast<size_t>(segment.id)])
            segment.route_ids.clear();  // unused: MergeThroughNodes removes it
}

} // namespace

// Joins the two segments meeting at a node that is no junction: exactly two
// segments touch it, and either both carry the same routes or every route
// passing there goes from one to the other -- none starts, ends or turns
// around there. Repeats until every node is a junction, a route
// end or a turn-around; then renumbers. Turn-arounds shorter than
// `min_turn_m` are jitter and removed first.
// @impl 0f87-6ace-13b5-2444
// @impl 0f87-6ace-3222-ba6b
void MergeThroughNodes(SegmentNetwork &net, double min_turn_m, const std::vector<RestPlace> &kept, double kept_radius_m)
{
    std::vector<bool> removed(net.segments.size(), false);
    // Going along a short segment and straight back along it is mapping
    // jitter near a node, not a turn-around (those need a spur of at least
    // min_turn_m): the pair goes, repeatedly (a zigzag cancels out). The
    // routes of each segment are then those still using it.
    for (NetworkRoute &route : net.routes) {
        std::vector<RouteStep> steps;
        for (const RouteStep &step : route.steps) {
            if (!steps.empty() && steps.back().segment == step.segment && steps.back().forward != step.forward &&
                net.segments[static_cast<size_t>(step.segment)].length_m < min_turn_m) {
                steps.pop_back();
                continue;
            }
            steps.push_back(step);
        }
        route.steps.swap(steps);
    }
    std::vector<std::set<std::string>> users(net.segments.size());
    for (const NetworkRoute &route : net.routes)
        for (const RouteStep &step : route.steps)
            users[static_cast<size_t>(step.segment)].insert(route.id);
    for (NetworkSegment &segment : net.segments) {
        segment.route_ids.assign(users[static_cast<size_t>(segment.id)].begin(), users[static_cast<size_t>(segment.id)].end());
        if (segment.route_ids.empty())
            removed[static_cast<size_t>(segment.id)] = true;
    }

    bool merged = true;
    while (merged) {
        merged = false;
        // Segments touching each node.
        std::vector<std::vector<int>> touching(net.nodes.size());
        for (const NetworkSegment &segment : net.segments) {
            if (removed[static_cast<size_t>(segment.id)])
                continue;
            touching[static_cast<size_t>(segment.from)].push_back(segment.id);
            touching[static_cast<size_t>(segment.to)].push_back(segment.id);
        }
        // Nodes where a route starts, ends or turns around.
        std::vector<bool> stop(net.nodes.size(), false);
        for (const NetworkRoute &route : net.routes) {
            if (route.steps.empty())
                continue;
            auto entry = [&](const RouteStep &step) {
                const NetworkSegment &segment = net.segments[static_cast<size_t>(step.segment)];
                return step.forward ? segment.from : segment.to;
            };
            auto exit = [&](const RouteStep &step) {
                const NetworkSegment &segment = net.segments[static_cast<size_t>(step.segment)];
                return step.forward ? segment.to : segment.from;
            };
            stop[static_cast<size_t>(entry(route.steps.front()))] = true;
            stop[static_cast<size_t>(exit(route.steps.back()))] = true;
            for (size_t i = 1; i < route.steps.size(); ++i)
                if (route.steps[i].segment == route.steps[i - 1].segment)
                    stop[static_cast<size_t>(exit(route.steps[i - 1]))] = true;  // turned back on it
        }
        for (size_t node = 0; node < net.nodes.size() && !merged; ++node) {
            const std::vector<int> &at = touching[node];
            if (at.size() != 2 || at[0] == at[1])
                continue;
            const NetworkNode &here = net.nodes[node];
            if (std::any_of(kept.begin(), kept.end(), [&](const RestPlace &place) {
                    return HaversineM(here.lat, here.lon, place.lat, place.lon) <= kept_radius_m;
                }))
                continue;  // a rest place: a boundary of its own
            NetworkSegment &first = net.segments[static_cast<size_t>(at[0])];
            NetworkSegment &second = net.segments[static_cast<size_t>(at[1])];
            // No branching and the same routes on both sides: one segment,
            // whatever happens at the node (a route turning back there also
            // uses the other side elsewhere). Otherwise every route must pass
            // straight through.
            const bool same_routes = std::set<std::string>(first.route_ids.begin(), first.route_ids.end()) ==
                                     std::set<std::string>(second.route_ids.begin(), second.route_ids.end());
            if (stop[node] && !same_routes)
                continue;
            // Orient: first ends at the node, second starts there.
            const bool first_reversed = first.to != static_cast<int>(node);
            const bool second_reversed = second.from != static_cast<int>(node);
            // Every use of either segment must be followed or preceded by the
            // other through this node.
            bool through = true;
            for (const NetworkRoute &route : net.routes)
                for (size_t i = 0; i < route.steps.size() && through; ++i) {
                    const RouteStep &step = route.steps[i];
                    if (step.segment != first.id && step.segment != second.id)
                        continue;
                    const int other = step.segment == first.id ? second.id : first.id;
                    const bool towards_node = step.segment == first.id ? step.forward != first_reversed
                                                                        : step.forward == second_reversed;
                    const size_t neighbour = towards_node ? i + 1 : i - 1;
                    if ((towards_node && i + 1 >= route.steps.size()) || (!towards_node && i == 0) ||
                        route.steps[neighbour].segment != other)
                        through = false;
                }
            if (!through && !same_routes)
                continue;
            // first (oriented to end at node) + second (oriented to start there).
            std::vector<double> lon = first.lon, lat = first.lat;
            if (first_reversed) {
                std::reverse(lon.begin(), lon.end());
                std::reverse(lat.begin(), lat.end());
            }
            std::vector<double> second_lon = second.lon, second_lat = second.lat;
            if (second_reversed) {
                std::reverse(second_lon.begin(), second_lon.end());
                std::reverse(second_lat.begin(), second_lat.end());
            }
            lon.insert(lon.end(), second_lon.begin() + 1, second_lon.end());
            lat.insert(lat.end(), second_lat.begin() + 1, second_lat.end());
            const int start = first_reversed ? first.to : first.from;
            const int end = second_reversed ? second.from : second.to;
            // The merged segment replaces `first`, oriented start -> end.
            for (NetworkRoute &route : net.routes) {
                std::vector<RouteStep> steps;
                for (size_t i = 0; i < route.steps.size(); ++i) {
                    const RouteStep &step = route.steps[i];
                    // Forward along the merged segment: travelling first
                    // towards the node, or second away from it.
                    RouteStep merged = step;
                    if (step.segment == first.id)
                        merged = {first.id, step.forward != first_reversed};
                    else if (step.segment == second.id)
                        merged = {first.id, step.forward != second_reversed};
                    // Passing through the node: the two steps are one.
                    if (!steps.empty() && merged.segment == first.id && steps.back().segment == first.id &&
                        steps.back().forward == merged.forward && (step.segment == second.id) != (route.steps[i - 1].segment == second.id))
                        continue;
                    steps.push_back(merged);
                }
                route.steps.swap(steps);
            }
            first.lon = std::move(lon);
            first.lat = std::move(lat);
            first.from = start;
            first.to = end;
            first.length_m += second.length_m;
            std::set<std::string> routes(first.route_ids.begin(), first.route_ids.end());
            routes.insert(second.route_ids.begin(), second.route_ids.end());
            first.route_ids.assign(routes.begin(), routes.end());
            removed[static_cast<size_t>(second.id)] = true;
            merged = true;
        }
    }
    // Renumber the remaining segments and the nodes still in use.
    std::vector<int> segment_id(net.segments.size(), -1);
    std::vector<NetworkSegment> segments;
    for (NetworkSegment &segment : net.segments) {
        if (removed[static_cast<size_t>(segment.id)])
            continue;
        segment_id[static_cast<size_t>(segment.id)] = static_cast<int>(segments.size());
        segment.id = static_cast<int>(segments.size());
        segments.push_back(std::move(segment));
    }
    std::vector<int> node_id(net.nodes.size(), -1);
    std::vector<NetworkNode> nodes;
    for (NetworkSegment &segment : segments)
        for (int *end : {&segment.from, &segment.to}) {
            int &id = node_id[static_cast<size_t>(*end)];
            if (id < 0) {
                id = static_cast<int>(nodes.size());
                NetworkNode node = net.nodes[static_cast<size_t>(*end)];
                node.id = id;
                nodes.push_back(node);
            }
            *end = id;
        }
    for (NetworkRoute &route : net.routes)
        for (RouteStep &step : route.steps)
            step.segment = segment_id[static_cast<size_t>(step.segment)];
    net.segments = std::move(segments);
    net.nodes = std::move(nodes);
}

// A loop shorter than `max_length` (a segment from a node back to itself)
// is a blob around a junction, whatever its routes: it goes.
bool DropShortLoops(SegmentNetwork &net, double max_length)
{
    std::vector<bool> drop(net.segments.size(), false);
    bool any = false;
    for (const NetworkSegment &segment : net.segments)
        if (segment.from == segment.to && segment.length_m < max_length && !segment.route_ids.empty()) {
            drop[static_cast<size_t>(segment.id)] = true;
            any = true;
        }
    if (any)
        RemoveSegments(net, drop);
    return any;
}

// Two segments between the same two nodes, the shorter lying along the
// longer within `max_distance`, are one path: the busier absorbs the other
// (its routes and steps).
bool MergeDuplicates(SegmentNetwork &net, double max_distance)
{
    auto shape = [](const NetworkSegment &segment, bool reversed) {
        std::vector<XY> line;
        for (size_t i = 0; i < segment.lat.size(); ++i)
            line.push_back({segment.lon[i] * 111320.0 * std::cos(segment.lat[i] * kPi / 180.0), segment.lat[i] * 110540.0});
        if (reversed)
            std::reverse(line.begin(), line.end());
        std::vector<XY> samples;
        const double length = Length(line);
        for (int j = 0; j < 20; ++j)
            samples.push_back(AlongFrom(line, 0, length * j / 19.0));
        return samples;
    };
    std::vector<size_t> order;
    for (size_t i = 0; i < net.segments.size(); ++i)
        if (!net.segments[i].route_ids.empty() && net.segments[i].from != net.segments[i].to)
            order.push_back(i);
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return net.segments[a].route_ids.size() > net.segments[b].route_ids.size();
    });
    std::vector<int> into(net.segments.size(), -1);
    std::vector<bool> reversed_into(net.segments.size(), false);
    bool any = false;
    for (size_t i = 0; i < order.size(); ++i) {
        const NetworkSegment &keep = net.segments[order[i]];
        if (into[order[i]] >= 0)
            continue;
        const std::vector<XY> keep_shape = shape(keep, false);
        for (size_t j = i + 1; j < order.size(); ++j) {
            const NetworkSegment &other = net.segments[order[j]];
            if (into[order[j]] >= 0)
                continue;
            const bool same = other.from == keep.from && other.to == keep.to;
            const bool opposite = other.from == keep.to && other.to == keep.from;
            if (!same && !opposite)
                continue;
            // Same path: the shorter of the two lies along the longer, every
            // point within max_distance of it. Lengths may differ (GPS
            // wandering while it gets a fix lengthens a start); two
            // different ways between the same nodes do not pass this.
            const std::vector<XY> other_shape = shape(other, opposite);
            const bool other_shorter = other.length_m <= keep.length_m;
            const std::vector<XY> &shorter = other_shorter ? other_shape : keep_shape;
            const std::vector<XY> &longer = other_shorter ? keep_shape : other_shape;
            double worst = 0.0;
            for (const XY &q : shorter)
                worst = std::max(worst, DistanceToLine(q, longer));
            if (worst > max_distance)
                continue;
            into[order[j]] = static_cast<int>(order[i]);
            reversed_into[order[j]] = opposite;
            any = true;
        }
    }
    // A segment can also duplicate a chain of two segments meeting at a
    // node in between (one route's way down matched to a straight line,
    // the others' way up through a junction): its routes are rerouted
    // through the chain, which keeps the junction.
    std::vector<int> via_first(net.segments.size(), -1), via_second(net.segments.size(), -1);
    std::vector<bool> via_first_forward(net.segments.size(), true), via_second_forward(net.segments.size(), true);
    for (size_t i = 0; i < net.segments.size(); ++i) {
        const NetworkSegment &lone = net.segments[i];
        if (into[i] >= 0 || lone.route_ids.empty() || lone.from == lone.to)
            continue;
        const std::vector<XY> lone_shape = shape(lone, false);
        bool found = false;
        for (size_t a = 0; a < net.segments.size() && !found; ++a) {
            const NetworkSegment &first = net.segments[a];
            if (a == i || into[a] >= 0 || first.route_ids.empty() || first.from == first.to ||
                (first.from != lone.from && first.to != lone.from))
                continue;
            const bool first_forward = first.from == lone.from;
            const int middle = first_forward ? first.to : first.from;
            if (middle == lone.to)
                continue;
            for (size_t b = 0; b < net.segments.size() && !found; ++b) {
                const NetworkSegment &second = net.segments[b];
                if (b == i || b == a || into[b] >= 0 || second.route_ids.empty())
                    continue;
                const bool second_forward = second.from == middle && second.to == lone.to;
                if (!second_forward && !(second.to == middle && second.from == lone.to))
                    continue;
                const double ratio = (first.length_m + second.length_m) / std::max(1.0, lone.length_m);
                if (ratio < 0.7 || ratio > 1.0 / 0.7)
                    continue;
                // The chain's shape, oriented from lone.from to lone.to.
                NetworkSegment chain;
                std::vector<double> lon1 = first.lon, lat1 = first.lat, lon2 = second.lon, lat2 = second.lat;
                if (!first_forward) {
                    std::reverse(lon1.begin(), lon1.end());
                    std::reverse(lat1.begin(), lat1.end());
                }
                if (!second_forward) {
                    std::reverse(lon2.begin(), lon2.end());
                    std::reverse(lat2.begin(), lat2.end());
                }
                chain.lon = lon1;
                chain.lat = lat1;
                chain.lon.insert(chain.lon.end(), lon2.begin(), lon2.end());
                chain.lat.insert(chain.lat.end(), lat2.begin(), lat2.end());
                const std::vector<XY> chain_shape = shape(chain, false);
                double sum = 0.0;
                for (size_t k = 0; k < lone_shape.size(); ++k)
                    sum += Dist(lone_shape[k], chain_shape[k]);
                if (sum / static_cast<double>(lone_shape.size()) > max_distance)
                    continue;
                via_first[i] = static_cast<int>(a);
                via_second[i] = static_cast<int>(b);
                via_first_forward[i] = first_forward;
                via_second_forward[i] = second_forward;
                found = true;
                any = true;
            }
        }
    }
    if (!any)
        return false;
    for (NetworkRoute &route : net.routes) {
        std::vector<RouteStep> steps;
        for (const RouteStep &step : route.steps) {
            const size_t id = static_cast<size_t>(step.segment);
            if (via_first[id] < 0) {
                steps.push_back(step);
                continue;
            }
            // Along lone forward: first then second, each in its chain
            // direction; backward: the reverse.
            const RouteStep one{via_first[id], via_first_forward[id]}, two{via_second[id], via_second_forward[id]};
            if (step.forward) {
                steps.push_back(one);
                steps.push_back(two);
            } else {
                steps.push_back({two.segment, !two.forward});
                steps.push_back({one.segment, !one.forward});
            }
        }
        route.steps.swap(steps);
    }
    for (size_t i = 0; i < net.segments.size(); ++i)
        if (via_first[i] >= 0) {
            for (int target : {via_first[i], via_second[i]}) {
                NetworkSegment &chain_part = net.segments[static_cast<size_t>(target)];
                std::set<std::string> routes(chain_part.route_ids.begin(), chain_part.route_ids.end());
                routes.insert(net.segments[i].route_ids.begin(), net.segments[i].route_ids.end());
                chain_part.route_ids.assign(routes.begin(), routes.end());
            }
            net.segments[i].route_ids.clear();
        }
    for (NetworkRoute &route : net.routes)
        for (RouteStep &step : route.steps) {
            const int target = into[static_cast<size_t>(step.segment)];
            if (target < 0)
                continue;
            if (reversed_into[static_cast<size_t>(step.segment)])
                step.forward = !step.forward;
            step.segment = target;
        }
    for (size_t i = 0; i < net.segments.size(); ++i)
        if (into[i] >= 0) {
            NetworkSegment &target = net.segments[static_cast<size_t>(into[i])];
            std::set<std::string> routes(target.route_ids.begin(), target.route_ids.end());
            routes.insert(net.segments[i].route_ids.begin(), net.segments[i].route_ids.end());
            target.route_ids.assign(routes.begin(), routes.end());
            net.segments[i].route_ids.clear();
        }
    return true;
}

void CleanUpNetwork(SegmentNetwork &network, const CleanupParams &params)
{
    // Rounds until nothing changes: junction blobs (short loops) go,
    // segments describing the same path between the same nodes merge, and
    // segments meet through nodes that are no longer junctions. Each can
    // make the next possible.
    for (int round = 0; round < 6; ++round) {
        MergeThroughNodes(network, params.min_turn_m, params.kept_places, params.kept_radius_m);
        if (!DropShortLoops(network, params.max_loop_m) && !MergeDuplicates(network, params.duplicate_distance_m))
            break;
    }
    MergeThroughNodes(network, params.min_turn_m, params.kept_places, params.kept_radius_m);
}

} // namespace openrider::sdk
