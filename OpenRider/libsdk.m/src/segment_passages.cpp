#include <openrider/sdk/segment_passages.h>

#include <openrider/sdk/local_projection.h>

#include <algorithm>
#include <cmath>

namespace openrider::sdk {

LinePosition PositionOnLine(const XY &q, const std::vector<XY> &line)
{
    LinePosition best;
    best.distance_m = 1e300;
    double walked = 0.0;
    for (size_t i = 1; i < line.size(); ++i) {
        const double dx = line[i].x - line[i - 1].x, dy = line[i].y - line[i - 1].y;
        const double length2 = dx * dx + dy * dy, length = std::sqrt(length2);
        double fraction = length2 > 0.0 ? ((q.x - line[i - 1].x) * dx + (q.y - line[i - 1].y) * dy) / length2 : 0.0;
        fraction = std::clamp(fraction, 0.0, 1.0);
        const double distance = std::hypot(line[i - 1].x + fraction * dx - q.x, line[i - 1].y + fraction * dy - q.y);
        if (distance < best.distance_m)
            best = {walked + fraction * length, distance};
        walked += length;
    }
    if (line.size() == 1)
        best = {0.0, Dist(q, line[0])};
    return best;
}

SegmentPassage PassageAlong(const std::vector<or_point> &points, size_t first, size_t last, bool forward,
                            const NetworkSegment &segment)
{
    SegmentPassage passage;
    passage.first = first;
    passage.last = last;
    passage.forward = forward;
    if (segment.lat.empty() || first > last || last >= points.size())
        return passage;
    const LocalProjection projection(segment.lon[segment.lon.size() / 2], segment.lat[segment.lat.size() / 2]);
    std::vector<XY> line;
    for (size_t i = 0; i < segment.lat.size(); ++i)
        line.push_back(projection.Forward(segment.lon[i], segment.lat[i]));
    double previous = forward ? 0.0 : Length(line);
    for (size_t i = first; i <= last; ++i) {
        if (!std::isnan(points[i].lat) && !std::isnan(points[i].lon))
            previous = PositionOnLine(projection.Forward(points[i].lon, points[i].lat), line).along_m;
        passage.along_m.push_back(previous);
    }
    return passage;
}

std::vector<SegmentPassage> FindPassages(const std::vector<or_point> &points, const NetworkSegment &segment,
                                         double max_distance_m, double min_coverage)
{
    std::vector<SegmentPassage> passages;
    if (segment.lat.size() < 2 || points.empty())
        return passages;
    const LocalProjection projection(segment.lon[segment.lon.size() / 2], segment.lat[segment.lat.size() / 2]);
    std::vector<XY> line;
    for (size_t i = 0; i < segment.lat.size(); ++i)
        line.push_back(projection.Forward(segment.lon[i], segment.lat[i]));
    const double length = Length(line);
    const double needed = min_coverage * length;
    // Position of every point along the line (NaN: no position, or too far).
    std::vector<double> along(points.size(), std::nan(""));
    for (size_t i = 0; i < points.size(); ++i) {
        if (std::isnan(points[i].lat) || std::isnan(points[i].lon))
            continue;
        const LinePosition position = PositionOnLine(projection.Forward(points[i].lon, points[i].lat), line);
        if (position.distance_m <= max_distance_m)
            along[i] = position.along_m;
    }
    // Stretches of near points; up to `bridge` far points in a row are
    // bridged (a GPS excursion, a point without position).
    const size_t bridge = 5;
    std::vector<std::pair<size_t, size_t>> stretches;
    for (size_t i = 0; i < points.size();) {
        if (std::isnan(along[i])) {
            ++i;
            continue;
        }
        size_t last = i, j = i + 1, gap = 0;
        for (; j < points.size() && gap <= bridge; ++j) {
            if (std::isnan(along[j])) {
                ++gap;
                continue;
            }
            gap = 0;
            last = j;
        }
        stretches.push_back({i, last});
        i = last + 1;
    }
    // Each stretch cut where it turns back along the line (by more than the
    // matching distance), then kept when it covers enough of the line.
    auto keep = [&](size_t first, size_t last) {
        double low = 1e300, high = -1e300;
        size_t first_near = last, last_near = first;
        for (size_t i = first; i <= last; ++i)
            if (!std::isnan(along[i])) {
                low = std::min(low, along[i]);
                high = std::max(high, along[i]);
                first_near = std::min(first_near, i);
                last_near = std::max(last_near, i);
            }
        if (first_near >= last_near || high - low < needed)
            return;
        SegmentPassage passage;
        passage.first = first_near;
        passage.last = last_near;
        passage.forward = along[last_near] >= along[first_near];
        double previous = along[first_near];
        for (size_t i = first_near; i <= last_near; ++i) {
            if (!std::isnan(along[i]))
                previous = along[i];
            passage.along_m.push_back(previous);  // bridged points keep the last position
        }
        passages.push_back(std::move(passage));
    };
    for (const auto &[first, last] : stretches) {
        size_t start = first;
        int direction = 0;          // +1 increasing along, -1 decreasing, 0 not yet known
        double extreme = along[first];
        size_t extreme_at = first;
        for (size_t i = first + 1; i <= last; ++i) {
            if (std::isnan(along[i]))
                continue;
            if (direction == 0) {
                if (std::fabs(along[i] - along[start]) > max_distance_m)
                    direction = along[i] > along[start] ? 1 : -1;
                extreme = along[i];
                extreme_at = i;
                continue;
            }
            if ((direction > 0 && along[i] >= extreme) || (direction < 0 && along[i] <= extreme)) {
                extreme = along[i];
                extreme_at = i;
            } else if (std::fabs(along[i] - extreme) > 2.0 * max_distance_m) {
                keep(start, extreme_at);  // turned back: the passage ends at the extreme
                start = extreme_at;
                direction = -direction;
                extreme = along[i];
                extreme_at = i;
            }
        }
        keep(start, last);
    }
    return passages;
}

} // namespace openrider::sdk
