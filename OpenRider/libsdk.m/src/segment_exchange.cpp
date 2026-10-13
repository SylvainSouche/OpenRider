#include <openrider/sdk/segment_exchange.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>

namespace openrider::sdk {

Result<SegmentNetwork> RunSegments(const or_segments_vtable *plugin, const std::vector<OverviewTrack> &tracks,
                                   const std::vector<double> &values, or_segment_cancel_cb cancel, void *user)
{
    std::vector<or_segment_track> input;
    for (const OverviewTrack &track : tracks) {
        or_segment_track entry{};
        entry.id = track.id.c_str();
        entry.count = track.lat.size();
        entry.lon = track.lon.data();
        entry.lat = track.lat.data();
        entry.noise_m = track.noise_m.size() == track.lat.size() ? track.noise_m.data() : nullptr;
        input.push_back(entry);
    }
    or_segment_network *built = nullptr;
    char error[256] = "";
    if (plugin->build(input.data(), input.size(), values.data(), cancel, user, &built, error, sizeof error) != 0)
        return Result<SegmentNetwork>::Failure(std::string(plugin->id) + ": " + error);
    if (!built)
        return Result<SegmentNetwork>::Failure(std::string(plugin->id) + ": no network returned");
    SegmentNetwork network = ImportNetwork(*built, tracks);
    plugin->free_network(built);
    return Result<SegmentNetwork>::Success(std::move(network));
}

SegmentNetwork ImportNetwork(const or_segment_network &network, const std::vector<OverviewTrack> &tracks)
{
    SegmentNetwork out;
    for (size_t n = 0; n < network.node_count; ++n)
        out.nodes.push_back({static_cast<int>(n), network.nodes[n].lon, network.nodes[n].lat});
    for (size_t s = 0; s < network.segment_count; ++s) {
        const or_segment &in = network.segments[s];
        NetworkSegment segment;
        segment.id = static_cast<int>(s);
        segment.from = static_cast<int>(in.from);
        segment.to = static_cast<int>(in.to);
        segment.lon.assign(in.lon, in.lon + in.count);
        segment.lat.assign(in.lat, in.lat + in.count);
        segment.length_m = in.length_m;
        for (size_t r = 0; r < in.route_count; ++r)
            if (in.routes[r] < tracks.size())
                segment.route_ids.push_back(tracks[in.routes[r]].id);
        out.segments.push_back(std::move(segment));
    }
    for (size_t r = 0; r < network.route_count && r < tracks.size(); ++r) {
        NetworkRoute route;
        route.id = tracks[r].id;
        for (size_t k = 0; k < network.routes[r].step_count; ++k) {
            const or_route_step &step = network.routes[r].steps[k];
            route.steps.push_back({static_cast<int>(step.segment), step.forward != 0, step.first, step.last});
        }
        out.routes.push_back(std::move(route));
    }
    return out;
}

namespace {

template <class T>
T *Allocate(size_t count)
{
    return static_cast<T *>(std::calloc(count ? count : 1, sizeof(T)));
}

} // namespace

or_segment_network *ExportNetwork(const SegmentNetwork &network, const std::vector<OverviewTrack> &tracks)
{
    std::map<std::string, uint32_t> index_of;
    for (size_t t = 0; t < tracks.size(); ++t)
        index_of[tracks[t].id] = static_cast<uint32_t>(t);
    or_segment_network *out = Allocate<or_segment_network>(1);
    out->node_count = network.nodes.size();
    out->nodes = Allocate<or_segment_node>(network.nodes.size());
    for (size_t n = 0; n < network.nodes.size(); ++n)
        out->nodes[n] = {network.nodes[n].lon, network.nodes[n].lat};
    out->segment_count = network.segments.size();
    out->segments = Allocate<or_segment>(network.segments.size());
    for (size_t s = 0; s < network.segments.size(); ++s) {
        const NetworkSegment &in = network.segments[s];
        or_segment &segment = out->segments[s];
        segment.from = static_cast<uint32_t>(in.from);
        segment.to = static_cast<uint32_t>(in.to);
        segment.count = in.lat.size();
        segment.lon = Allocate<double>(in.lon.size());
        segment.lat = Allocate<double>(in.lat.size());
        std::copy(in.lon.begin(), in.lon.end(), segment.lon);
        std::copy(in.lat.begin(), in.lat.end(), segment.lat);
        segment.length_m = in.length_m;
        segment.routes = Allocate<uint32_t>(in.route_ids.size());
        for (const std::string &id : in.route_ids) {
            const auto found = index_of.find(id);
            if (found != index_of.end())
                segment.routes[segment.route_count++] = found->second;
        }
    }
    // One route per input track, in input order.
    out->route_count = tracks.size();
    out->routes = Allocate<or_segment_route>(tracks.size());
    for (const NetworkRoute &route : network.routes) {
        const auto found = index_of.find(route.id);
        if (found == index_of.end())
            continue;
        or_segment_route &target = out->routes[found->second];
        std::free(target.steps);
        target.step_count = route.steps.size();
        target.steps = Allocate<or_route_step>(route.steps.size());
        for (size_t k = 0; k < route.steps.size(); ++k)
            target.steps[k] = {static_cast<uint32_t>(route.steps[k].segment), route.steps[k].forward ? 1u : 0u,
                               static_cast<uint32_t>(route.steps[k].first), static_cast<uint32_t>(route.steps[k].last)};
    }
    return out;
}

void FreeExportedNetwork(or_segment_network *network)
{
    if (!network)
        return;
    for (size_t s = 0; s < network->segment_count; ++s) {
        std::free(network->segments[s].lon);
        std::free(network->segments[s].lat);
        std::free(network->segments[s].routes);
    }
    for (size_t r = 0; r < network->route_count; ++r)
        std::free(network->routes[r].steps);
    std::free(network->nodes);
    std::free(network->segments);
    std::free(network->routes);
    std::free(network);
}

std::vector<OverviewTrack> ImportTracks(const or_segment_track *tracks, size_t count)
{
    std::vector<OverviewTrack> out;
    for (size_t t = 0; t < count; ++t) {
        OverviewTrack track;
        track.id = tracks[t].id ? tracks[t].id : "";
        track.lon.assign(tracks[t].lon, tracks[t].lon + tracks[t].count);
        track.lat.assign(tracks[t].lat, tracks[t].lat + tracks[t].count);
        if (tracks[t].noise_m)
            track.noise_m.assign(tracks[t].noise_m, tracks[t].noise_m + tracks[t].count);
        track.west = track.south = 1e9;
        track.east = track.north = -1e9;
        for (size_t i = 0; i < track.lat.size(); ++i) {
            track.west = std::min(track.west, track.lon[i]);
            track.east = std::max(track.east, track.lon[i]);
            track.south = std::min(track.south, track.lat[i]);
            track.north = std::max(track.north, track.lat[i]);
        }
        out.push_back(std::move(track));
    }
    return out;
}

} // namespace openrider::sdk
