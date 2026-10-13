#include "app/comparison_domain.h"

#include "core/log.h"

#include <openrider/sdk/segment_passages.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace app {

namespace {

constexpr double kPassageDistanceM = 30.0;  // a point this close to the segment's line is on it
constexpr double kPassageCoverage = 0.8;    // a passage covers this much of the segment at least

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
    // Every passage of each outing along the segment, from its recorded
    // points (loaded now: a few dozen outings at most).
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
        for (openrider::sdk::SegmentPassage &passage : openrider::sdk::FindPassages(points, comparison.segment, kPassageDistanceM, kPassageCoverage)) {
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
