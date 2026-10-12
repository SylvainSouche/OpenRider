#include "core/segments.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <limits>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace core {

namespace {

constexpr double kPi = 3.14159265358979323846;

// Local equirectangular projection, metres; fine at a regional scale.
struct Proj {
    double lat0 = 0, lon0 = 0, kx = 1, ky = 1;
    void fwd(double lon, double lat, double &x, double &y) const
    {
        x = (lon - lon0) * kx;
        y = (lat - lat0) * ky;
    }
    void inv(double x, double y, double &lon, double &lat) const
    {
        lon = lon0 + x / kx;
        lat = lat0 + y / ky;
    }
};

struct XY {
    double x, y;
};

using Cell = int64_t;
Cell key(int64_t ix, int64_t iy) { return (ix << 32) ^ (iy & 0xffffffffLL); }
int64_t key_x(Cell c) { return c >> 32; }
int64_t key_y(Cell c) { return static_cast<int32_t>(c & 0xffffffffLL); }

double dist(const XY &a, const XY &b) { return std::hypot(a.x - b.x, a.y - b.y); }

double length(const std::vector<XY> &l)
{
    double s = 0.0;
    for (size_t i = 1; i < l.size(); ++i)
        s += dist(l[i - 1], l[i]);
    return s;
}

// Points every `step` metres along a polyline (ends included).
std::vector<XY> densify(const std::vector<XY> &l, double step)
{
    std::vector<XY> out;
    if (l.empty())
        return out;
    out.push_back(l[0]);
    double carry = 0.0;
    for (size_t i = 1; i < l.size(); ++i) {
        const double d = dist(l[i - 1], l[i]);
        double s = step - carry;
        while (s <= d) {
            const double t = s / d;
            out.push_back({l[i - 1].x + (l[i].x - l[i - 1].x) * t, l[i - 1].y + (l[i].y - l[i - 1].y) * t});
            s += step;
        }
        carry = d - (s - step);
    }
    if (dist(out.back(), l.back()) > 1e-6)
        out.push_back(l.back());
    return out;
}

void douglas_peucker(const std::vector<XY> &l, size_t a, size_t b, double eps, std::vector<bool> &keep)
{
    if (b <= a + 1)
        return;
    const double dx = l[b].x - l[a].x, dy = l[b].y - l[a].y;
    const double len = std::hypot(dx, dy);
    double best = -1.0;
    size_t idx = a;
    for (size_t i = a + 1; i < b; ++i) {
        const double d = len > 0 ? std::fabs(dy * (l[i].x - l[a].x) - dx * (l[i].y - l[a].y)) / len
                                 : dist(l[i], l[a]);
        if (d > best) {
            best = d;
            idx = i;
        }
    }
    if (best > eps) {
        keep[idx] = true;
        douglas_peucker(l, a, idx, eps, keep);
        douglas_peucker(l, idx, b, eps, keep);
    }
}

std::vector<XY> simplify(const std::vector<XY> &l, double eps)
{
    if (l.size() < 3)
        return l;
    std::vector<bool> keep(l.size(), false);
    keep.front() = keep.back() = true;
    douglas_peucker(l, 0, l.size() - 1, eps, keep);
    std::vector<XY> out;
    for (size_t i = 0; i < l.size(); ++i)
        if (keep[i])
            out.push_back(l[i]);
    return out;
}

struct Edge {
    int a = -1, b = -1;  // node ids (-1: open end)
    std::vector<XY> pts;
    bool removed = false;
};

// Skeleton of one corridor component -> edges (in metres).
void trace_component(const std::vector<Cell> &cells, double cell, std::vector<Edge> &edges, int &next_node)
{
    int64_t x0 = INT64_MAX, y0 = INT64_MAX, x1 = INT64_MIN, y1 = INT64_MIN;
    for (Cell c : cells) {
        x0 = std::min(x0, key_x(c));
        x1 = std::max(x1, key_x(c));
        y0 = std::min(y0, key_y(c));
        y1 = std::max(y1, key_y(c));
    }
    const int w = static_cast<int>(x1 - x0 + 3), h = static_cast<int>(y1 - y0 + 3);
    std::vector<unsigned char> img(static_cast<size_t>(w) * h, 0);
    for (Cell c : cells)
        img[static_cast<size_t>(key_y(c) - y0 + 1) * w + (key_x(c) - x0 + 1)] = 1;
    thin_image(img, w, h);

    auto at = [&](int x, int y) -> int {
        return x < 0 || y < 0 || x >= w || y >= h ? 0 : img[static_cast<size_t>(y) * w + x];
    };
    static const int nx[8] = {0, 1, 1, 1, 0, -1, -1, -1};  // clockwise from north
    static const int ny[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    // Crossing number: 0->1 transitions around the pixel. 1 = end, 2 = line,
    // >= 3 = junction (robust to staircase pixels, unlike a neighbour count).
    auto crossing = [&](int x, int y) {
        int c = 0;
        for (int k = 0; k < 8; ++k)
            if (!at(x + nx[k], y + ny[k]) && at(x + nx[(k + 1) % 8], y + ny[(k + 1) % 8]))
                ++c;
        return c;
    };
    std::vector<int> node(static_cast<size_t>(w) * h, -1);
    // Junction pixels touching each other form one node.
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            if (!at(x, y) || node[static_cast<size_t>(y) * w + x] >= 0)
                continue;
            const int cn = crossing(x, y);
            if (cn == 2 || cn == 0)
                continue;
            const int id = next_node++;
            std::deque<std::pair<int, int>> q{{x, y}};
            node[static_cast<size_t>(y) * w + x] = id;
            while (!q.empty() && cn >= 3) {
                auto [cx, cy] = q.front();
                q.pop_front();
                for (int k = 0; k < 8; ++k) {
                    const int ax = cx + nx[k], ay = cy + ny[k];
                    if (at(ax, ay) && node[static_cast<size_t>(ay) * w + ax] < 0 && crossing(ax, ay) >= 3) {
                        node[static_cast<size_t>(ay) * w + ax] = id;
                        q.push_back({ax, ay});
                    }
                }
            }
        }
    auto to_xy = [&](int x, int y) {
        return XY{(static_cast<double>(x0 + x - 1) + 0.5) * cell, (static_cast<double>(y0 + y - 1) + 0.5) * cell};
    };
    std::vector<unsigned char> used(static_cast<size_t>(w) * h, 0);
    auto walk = [&](int sx, int sy, int fx, int fy, int start_node) {
        Edge e;
        e.a = start_node;
        e.pts.push_back(to_xy(sx, sy));
        int px = sx, py = sy, cx = fx, cy = fy;
        for (;;) {
            e.pts.push_back(to_xy(cx, cy));
            const int nid = node[static_cast<size_t>(cy) * w + cx];
            if (nid >= 0) {
                e.b = nid;
                break;
            }
            used[static_cast<size_t>(cy) * w + cx] = 1;
            int bx = -1, by = -1;
            // Prefer a node, then 4-neighbours, then diagonals.
            for (int pass = 0; pass < 3 && bx < 0; ++pass)
                for (int k = 0; k < 8; ++k) {
                    if (pass == 1 && k % 2)
                        continue;
                    const int ax = cx + nx[k], ay = cy + ny[k];
                    if (!at(ax, ay) || (ax == px && ay == py))
                        continue;
                    const size_t ai = static_cast<size_t>(ay) * w + ax;
                    if (pass == 0 ? (node[ai] >= 0 && node[ai] != start_node) || (node[ai] >= 0 && e.pts.size() > 3)
                                  : (node[ai] < 0 && !used[ai])) {
                        bx = ax;
                        by = ay;
                        break;
                    }
                }
            if (bx < 0)
                break;  // dead end without a node
            px = cx;
            py = cy;
            cx = bx;
            cy = by;
        }
        if (e.pts.size() >= 2)
            edges.push_back(std::move(e));
    };
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const int nid = node[static_cast<size_t>(y) * w + x];
            if (nid < 0)
                continue;
            for (int k = 0; k < 8; ++k) {
                const int ax = x + nx[k], ay = y + ny[k];
                if (!at(ax, ay))
                    continue;
                const size_t ai = static_cast<size_t>(ay) * w + ax;
                if (node[ai] < 0 && !used[ai])
                    walk(x, y, ax, ay, nid);
            }
        }
    // Pure loops (no node at all): start anywhere.
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const size_t i = static_cast<size_t>(y) * w + x;
            if (!at(x, y) || node[i] >= 0 || used[i])
                continue;
            const int id = next_node++;
            node[i] = id;
            for (int k = 0; k < 8; ++k) {
                const int ax = x + nx[k], ay = y + ny[k];
                if (at(ax, ay) && node[static_cast<size_t>(ay) * w + ax] < 0 && !used[static_cast<size_t>(ay) * w + ax]) {
                    walk(x, y, ax, ay, id);
                    break;
                }
            }
        }
}

// Removes short dead ends, then joins edges through nodes of degree 2.
void clean_graph(std::vector<Edge> &edges, double min_spur)
{
    auto degrees = [&]() {
        std::map<int, int> d;
        for (const Edge &e : edges)
            if (!e.removed) {
                if (e.a >= 0)
                    ++d[e.a];
                if (e.b >= 0)
                    ++d[e.b];
            }
        return d;
    };
    for (int round = 0; round < 4; ++round) {
        bool changed = false;
        std::map<int, int> d = degrees();
        for (Edge &e : edges) {
            if (e.removed)
                continue;
            const bool dead_a = e.a < 0 || d[e.a] == 1, dead_b = e.b < 0 || d[e.b] == 1;
            if ((dead_a != dead_b) && length(e.pts) < min_spur) {
                e.removed = true;
                changed = true;
            }
        }
        if (!changed)
            break;
    }
    // Join through degree-2 nodes.
    for (bool merged = true; merged;) {
        merged = false;
        std::map<int, int> d = degrees();
        for (auto &[n, deg] : d) {
            if (deg != 2)
                continue;
            int i = -1, j = -1;
            for (int k = 0; k < static_cast<int>(edges.size()); ++k) {
                const Edge &e = edges[static_cast<size_t>(k)];
                if (e.removed || (e.a != n && e.b != n))
                    continue;
                (i < 0 ? i : j) = k;
            }
            if (i < 0 || j < 0 || i == j)
                continue;
            Edge &ei = edges[static_cast<size_t>(i)];
            Edge &ej = edges[static_cast<size_t>(j)];
            if (ei.b != n) {
                std::reverse(ei.pts.begin(), ei.pts.end());
                std::swap(ei.a, ei.b);
            }
            if (ej.a != n) {
                std::reverse(ej.pts.begin(), ej.pts.end());
                std::swap(ej.a, ej.b);
            }
            ei.pts.insert(ei.pts.end(), ej.pts.begin() + 1, ej.pts.end());
            ei.b = ej.b;
            ej.removed = true;
            merged = true;
            break;
        }
    }
}

} // namespace

void thin_image(std::vector<unsigned char> &img, int w, int h)
{
    auto at = [&](int x, int y) -> int {
        return x < 0 || y < 0 || x >= w || y >= h ? 0 : img[static_cast<size_t>(y) * w + x];
    };
    std::vector<size_t> clear;
    for (bool changed = true; changed;) {
        changed = false;
        for (int step = 0; step < 2; ++step) {
            clear.clear();
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x) {
                    if (!at(x, y))
                        continue;
                    const int p2 = at(x, y - 1), p3 = at(x + 1, y - 1), p4 = at(x + 1, y), p5 = at(x + 1, y + 1),
                              p6 = at(x, y + 1), p7 = at(x - 1, y + 1), p8 = at(x - 1, y), p9 = at(x - 1, y - 1);
                    const int b = p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9;
                    if (b < 2 || b > 6)
                        continue;
                    const int seq[9] = {p2, p3, p4, p5, p6, p7, p8, p9, p2};
                    int a = 0;
                    for (int k = 0; k < 8; ++k)
                        a += !seq[k] && seq[k + 1];
                    if (a != 1)
                        continue;
                    if (step == 0 ? (p2 * p4 * p6 || p4 * p6 * p8) : (p2 * p4 * p8 || p2 * p6 * p8))
                        continue;
                    clear.push_back(static_cast<size_t>(y) * w + x);
                }
            for (size_t i : clear)
                img[i] = 0;
            changed = changed || !clear.empty();
        }
    }
}

// @impl 0f87-6acd-6028-c636
SegmentResult find_segments(const std::vector<OverviewTrack> &tracks, const SegmentParams &p)
{
    SegmentResult out;
    if (tracks.empty())
        return out;
    // 1. Projection around the tracks' centre.
    Proj pr;
    double w = 1e9, e = -1e9, s = 1e9, n = -1e9;
    for (const OverviewTrack &t : tracks) {
        w = std::min(w, t.west);
        e = std::max(e, t.east);
        s = std::min(s, t.south);
        n = std::max(n, t.north);
    }
    pr.lat0 = (s + n) / 2.0;
    pr.lon0 = (w + e) / 2.0;
    pr.kx = 111320.0 * std::cos(pr.lat0 * kPi / 180.0);
    pr.ky = 110540.0;
    std::vector<std::vector<XY>> xy(tracks.size());
    for (size_t t = 0; t < tracks.size(); ++t)
        for (size_t i = 0; i < tracks[t].lat.size(); ++i) {
            XY q;
            pr.fwd(tracks[t].lon[i], tracks[t].lat[i], q.x, q.y);
            xy[t].push_back(q);
        }

    // @impl 0f87-6acd-60a9-7bf6
    // 2. Blurred heat map: 1 on the cells a track's line crosses, 0.5 on
    // their neighbours, once per track (the higher value); tracks add up.
    const double cell = p.cell_m, r = p.tolerance_m;
    std::unordered_map<Cell, double> heat;
    std::unordered_map<Cell, int> reached;  // distinct tracks per cell
    for (const std::vector<XY> &l : xy) {
        std::unordered_map<Cell, double> mine;
        for (const XY &q : densify(l, cell * 0.5)) {
            const int64_t ix = static_cast<int64_t>(std::floor(q.x / cell));
            const int64_t iy = static_cast<int64_t>(std::floor(q.y / cell));
            mine[key(ix, iy)] = 1.0;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    double &v = mine[key(ix + dx, iy + dy)];
                    v = std::max(v, 0.5);
                }
        }
        for (const auto &[c, v] : mine) {
            heat[c] += v;
            ++reached[c];
        }
    }

    // 3-4. Corridor components -> skeleton -> edges.
    std::unordered_set<Cell> corridor;
    for (const auto &[c, v] : heat)
        if (v >= p.min_density && reached[c] >= p.min_tracks)
            corridor.insert(c);
    std::vector<Edge> edges;
    int next_node = 0;
    std::unordered_set<Cell> seen;
    for (Cell start : corridor) {
        if (seen.count(start))
            continue;
        std::vector<Cell> comp;
        std::deque<Cell> q{start};
        seen.insert(start);
        while (!q.empty()) {
            const Cell c = q.front();
            q.pop_front();
            comp.push_back(c);
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    const Cell nb = key(key_x(c) + dx, key_y(c) + dy);
                    if (corridor.count(nb) && seen.insert(nb).second)
                        q.push_back(nb);
                }
        }
        trace_component(comp, cell, edges, next_node);
    }
    clean_graph(edges, p.min_spur_m);
    std::vector<std::vector<XY>> lines;  // densified skeleton per kept edge
    for (Edge &ed : edges)
        if (!ed.removed && length(ed.pts) >= p.min_spur_m * 0.5)
            lines.push_back(densify(simplify(ed.pts, cell * 0.5), cell));

    // 5. Map tracks: nearest skeleton vertex within tolerance (grid index).
    const double bucket = std::max(10.0, r * 2.0);
    std::unordered_map<Cell, std::vector<std::pair<int, int>>> index;
    for (int li = 0; li < static_cast<int>(lines.size()); ++li)
        for (int vi = 0; vi < static_cast<int>(lines[static_cast<size_t>(li)].size()); ++vi) {
            const XY &v = lines[static_cast<size_t>(li)][static_cast<size_t>(vi)];
            index[key(static_cast<int64_t>(std::floor(v.x / bucket)), static_cast<int64_t>(std::floor(v.y / bucket)))]
                .push_back({li, vi});
        }
    const double snap = r * 1.5;
    struct Match {
        int line = -1, vertex = -1;
        double offset = 0.0;  // signed distance from the skeleton
    };
    std::vector<std::vector<Match>> matches(tracks.size());
    for (size_t t = 0; t < tracks.size(); ++t) {
        for (const XY &q : xy[t]) {
            Match m;
            double best = snap;
            const int64_t bx = static_cast<int64_t>(std::floor(q.x / bucket)), by = static_cast<int64_t>(std::floor(q.y / bucket));
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    auto it = index.find(key(bx + dx, by + dy));
                    if (it == index.end())
                        continue;
                    for (auto [li, vi] : it->second) {
                        const XY &v = lines[static_cast<size_t>(li)][static_cast<size_t>(vi)];
                        const double d = dist(v, q);
                        if (d < best) {
                            best = d;
                            m.line = li;
                            m.vertex = vi;
                        }
                    }
                }
            if (m.line >= 0) {
                const std::vector<XY> &l = lines[static_cast<size_t>(m.line)];
                const size_t a = static_cast<size_t>(std::max(0, m.vertex - 1));
                const size_t b = std::min(l.size() - 1, static_cast<size_t>(m.vertex + 1));
                const double tx = l[b].x - l[a].x, ty = l[b].y - l[a].y, tl = std::hypot(tx, ty);
                const XY &v = l[static_cast<size_t>(m.vertex)];
                m.offset = tl > 0 ? ((q.x - v.x) * -ty + (q.y - v.y) * tx) / tl : 0.0;
            }
            matches[t].push_back(m);
        }
    }

    // Pieces: runs on one line, long enough; the rest is the track's own.
    std::vector<std::set<size_t>> line_tracks(lines.size());
    std::vector<std::vector<TrackPiece>> pieces(tracks.size());
    for (size_t t = 0; t < tracks.size(); ++t) {
        const std::vector<Match> &ms = matches[t];
        size_t i = 0;
        while (i < ms.size()) {
            size_t j = i;
            while (j + 1 < ms.size() && ms[j + 1].line == ms[i].line)
                ++j;
            TrackPiece pc{ms[i].line, i, j};
            double len = 0.0;
            for (size_t k2 = i + 1; k2 <= j; ++k2)
                len += dist(xy[t][k2 - 1], xy[t][k2]);
            if (pc.segment >= 0 && len < p.min_piece_m)
                pc.segment = -1;
            if (!pieces[t].empty() && pieces[t].back().segment == pc.segment)
                pieces[t].back().last = j;
            else
                pieces[t].push_back(pc);
            if (pc.segment >= 0)
                line_tracks[static_cast<size_t>(pc.segment)].insert(t);
            i = j + 1;
        }
    }

    // 6. Common segments and their mean tracks.
    std::vector<int> new_id(lines.size(), -1);
    for (size_t li = 0; li < lines.size(); ++li) {
        if (static_cast<int>(line_tracks[li].size()) < p.min_tracks)
            continue;
        const std::vector<XY> &l = lines[li];
        // Offset per vertex: mean over tracks of each track's mean offset.
        std::vector<std::map<size_t, std::pair<double, int>>> per(l.size());
        for (size_t t : line_tracks[li])
            for (const TrackPiece &pc : pieces[t])
                if (pc.segment == static_cast<int>(li))
                    for (size_t k2 = pc.first; k2 <= pc.last; ++k2) {
                        const Match &m = matches[t][k2];
                        auto &acc = per[static_cast<size_t>(m.vertex)][t];
                        acc.first += m.offset;
                        ++acc.second;
                    }
        std::vector<double> off(l.size(), std::numeric_limits<double>::quiet_NaN());
        for (size_t v = 0; v < l.size(); ++v) {
            double sum = 0.0;
            int nt = 0;
            for (const auto &[t, acc] : per[v]) {
                sum += acc.first / acc.second;
                ++nt;
            }
            if (nt > 0)
                off[v] = sum / nt;
        }
        // Smooth and fill gaps over +/- 4 vertices (about 20 m).
        Segment sg;
        sg.id = static_cast<int>(out.segments.size());
        std::vector<XY> mean;
        for (size_t v = 0; v < l.size(); ++v) {
            double sum = 0.0;
            int nn = 0;
            for (size_t u = v >= 4 ? v - 4 : 0; u <= std::min(l.size() - 1, v + 4); ++u)
                if (!std::isnan(off[u])) {
                    sum += off[u];
                    ++nn;
                }
            const double o = nn ? sum / nn : 0.0;
            const size_t a = v ? v - 1 : 0, b = std::min(l.size() - 1, v + 1);
            const double tx = l[b].x - l[a].x, ty = l[b].y - l[a].y, tl = std::hypot(tx, ty);
            mean.push_back(tl > 0 ? XY{l[v].x - ty / tl * o, l[v].y + tx / tl * o} : l[v]);
        }
        mean = simplify(mean, cell * 0.3);
        sg.length_m = length(mean);
        for (const XY &q : mean) {
            double lon, lat;
            pr.inv(q.x, q.y, lon, lat);
            sg.lon.push_back(lon);
            sg.lat.push_back(lat);
        }
        for (size_t t : line_tracks[li])
            sg.track_ids.push_back(tracks[t].id);
        new_id[li] = sg.id;
        out.segments.push_back(std::move(sg));
    }
    for (size_t t = 0; t < tracks.size(); ++t) {
        TrackMapping tm;
        tm.id = tracks[t].id;
        for (TrackPiece pc : pieces[t]) {
            pc.segment = pc.segment >= 0 ? new_id[static_cast<size_t>(pc.segment)] : -1;
            if (!tm.pieces.empty() && tm.pieces.back().segment == pc.segment)
                tm.pieces.back().last = pc.last;
            else
                tm.pieces.push_back(pc);
        }
        out.mappings.push_back(std::move(tm));
    }
    return out;
}

} // namespace core
