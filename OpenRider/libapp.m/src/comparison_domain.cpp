#include "app/comparison_domain.h"

#include "core/log.h"

#include <openrider/sdk/segment_passages.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace app {

namespace {

// For a network without step ranges only: passages found from geometry.
constexpr double kPassageDistanceM = 30.0;  // a point this close to the segment's line is on it
constexpr double kPassageCoverage = 0.8;    // a passage covers this much of the segment at least

/// The passages of outing `id` (its recorded `points`) along segment
/// `segment` of `network`: the steps of its route on that segment, their
/// ranges mapped from the thinned track the network was built from
/// (`tracks`) to the outing's points. Found from geometry when the network
/// has no step ranges.
std::vector<openrider::sdk::SegmentPassage> PassagesOf(const std::string &id, int segment,
                                                       const openrider::sdk::SegmentNetwork &network,
                                                       const std::vector<openrider::sdk::OverviewTrack> &tracks,
                                                       const std::vector<or_point> &points)
{
    const openrider::sdk::NetworkSegment &line = network.segments[static_cast<size_t>(segment)];
    const auto track = std::find_if(tracks.begin(), tracks.end(), [&](const auto &t) { return t.id == id; });
    const auto route = std::find_if(network.routes.begin(), network.routes.end(), [&](const auto &r) { return r.id == id; });
    const bool ranged = track != tracks.end() && route != network.routes.end() && !track->source_index.empty() &&
                        std::any_of(route->steps.begin(), route->steps.end(), [](const auto &s) { return s.last > s.first; });
    if (!ranged)
        return openrider::sdk::FindPassages(points, line, kPassageDistanceM, kPassageCoverage);
    std::vector<openrider::sdk::SegmentPassage> passages;
    for (const openrider::sdk::RouteStep &step : route->steps) {
        if (step.segment != segment || step.last >= track->source_index.size())
            continue;
        const size_t first = track->source_index[step.first], last = track->source_index[step.last];
        if (first < last && last < points.size())
            passages.push_back(openrider::sdk::PassageAlong(points, first, last, step.forward, line));
    }
    return passages;
}

} // namespace

bool ComparisonDomain::Handles(IntentKind kind) const
{
    return kind == IntentKind::OpenSegmentComparison || kind == IntentKind::CloseSegmentComparison;
}

void ComparisonDomain::Handle(const Intent &intent, Intents &)
{
    if (intent.kind == IntentKind::OpenSegmentComparison) {
        Open(intent.i, intent.j);
    } else if (intent.kind == IntentKind::CloseSegmentComparison) {
        model_.comparison.open = false;
        model_.comparison.runs.clear();
    }
}

// @impl 0f87-6ace-35a8-5c96
void ComparisonDomain::Open(int group, int segment)
{
    if (!input_.Ready() || group < 0 || group >= static_cast<int>(model_.segment_groups.size()))
        return;
    const openrider::sdk::SegmentNetwork &network = model_.segment_groups[static_cast<size_t>(group)].network;
    if (segment < 0 || segment >= static_cast<int>(network.segments.size()))
        return;
    SegmentComparison &comparison = model_.comparison;
    comparison = SegmentComparison{true, comparison.generation + 1, model_.segment_groups[static_cast<size_t>(group)].sport,
                                   network.segments[static_cast<size_t>(segment)], {}};
    // Every passage of each outing along the segment: its route's steps on
    // the segment, over its recorded points (loaded now: a few dozen
    // outings at most).
    for (const std::string &id : comparison.segment.route_ids) {
        size_t outing = model_.outings.size();
        for (size_t i = 0; i < model_.outings.size(); ++i)
            if (id == model_.outings[i].raw.id)
                outing = i;
        if (outing == model_.outings.size())
            continue;
        std::vector<or_point> points;
        if (!input_.LoadPoints(id, points).Ok())
            continue;
        int pass = 0;
        for (openrider::sdk::SegmentPassage &passage : PassagesOf(id, segment, network, model_.tracks, points)) {
            ComparedRun run;
            run.outing = outing;
            run.pass = ++pass;
            run.forward = passage.forward;
            run.points.assign(points.begin() + static_cast<long>(passage.first),
                              points.begin() + static_cast<long>(passage.last) + 1);
            // Time and distance from the entry into the segment.
            const double t0 = run.points.front().t_s, d0 = run.points.front().dist_m;
            for (or_point &point : run.points) {
                point.t_s -= t0;
                if (!std::isnan(d0))
                    point.dist_m -= d0;
            }
            run.duration_s = run.points.back().t_s;
            run.along_m = std::move(passage.along_m);
            comparison.runs.push_back(std::move(run));
        }
    }
    std::stable_sort(comparison.runs.begin(), comparison.runs.end(), [&](const ComparedRun &a, const ComparedRun &b) {
        const int order = std::strcmp(model_.outings[a.outing].raw.start_local, model_.outings[b.outing].raw.start_local);
        return order != 0 ? order > 0 : a.pass < b.pass;
    });
    core::LogLine("comparing segment %d: %zu passages", segment, comparison.runs.size());
}

} // namespace app
