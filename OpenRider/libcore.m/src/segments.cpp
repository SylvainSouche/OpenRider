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
    void Fwd(double lon, double lat, double &x, double &y) const
    {
        x = (lon - lon0) * kx;
        y = (lat - lat0) * ky;
    }
    void Inv(double x, double y, double &lon, double &lat) const
    {
        lon = lon0 + x / kx;
        lat = lat0 + y / ky;
    }
};

struct XY {
    double x, y;
};

using Cell = int64_t;
Cell Key(int64_t ix, int64_t iy) { return (ix << 32) ^ (iy & 0xffffffffLL); }
int64_t KeyX(Cell c) { return c >> 32; }
int64_t KeyY(Cell c) { return static_cast<int32_t>(c & 0xffffffffLL); }

double Dist(const XY &a, const XY &b) { return std::hypot(a.x - b.x, a.y - b.y); }

double Length(const std::vector<XY> &l)
{
    double s = 0.0;
    for (size_t i = 1; i < l.size(); ++i)
        s += Dist(l[i - 1], l[i]);
    return s;
}

// Points every `step` metres along a polyline (ends included).
std::vector<XY> Densify(const std::vector<XY> &l, double step)
{
    std::vector<XY> out;
    if (l.empty())
        return out;
    out.push_back(l[0]);
    double carry = 0.0;
    for (size_t i = 1; i < l.size(); ++i) {
        const double d = Dist(l[i - 1], l[i]);
        double s = step - carry;
        while (s <= d) {
            const double t = s / d;
            out.push_back({l[i - 1].x + (l[i].x - l[i - 1].x) * t, l[i - 1].y + (l[i].y - l[i - 1].y) * t});
            s += step;
        }
        carry = d - (s - step);
    }
    if (Dist(out.back(), l.back()) > 1e-6)
        out.push_back(l.back());
    return out;
}

void DouglasPeucker(const std::vector<XY> &l, size_t a, size_t b, double eps, std::vector<bool> &keep)
{
    if (b <= a + 1)
        return;
    const double dx = l[b].x - l[a].x, dy = l[b].y - l[a].y;
    const double len = std::hypot(dx, dy);
    double best = -1.0;
    size_t idx = a;
    for (size_t i = a + 1; i < b; ++i) {
        const double d = len > 0 ? std::fabs(dy * (l[i].x - l[a].x) - dx * (l[i].y - l[a].y)) / len
                                 : Dist(l[i], l[a]);
        if (d > best) {
            best = d;
            idx = i;
        }
    }
    if (best > eps) {
        keep[idx] = true;
        DouglasPeucker(l, a, idx, eps, keep);
        DouglasPeucker(l, idx, b, eps, keep);
    }
}

std::vector<XY> Simplify(const std::vector<XY> &l, double eps)
{
    if (l.size() < 3)
        return l;
    std::vector<bool> keep(l.size(), false);
    keep.front() = keep.back() = true;
    DouglasPeucker(l, 0, l.size() - 1, eps, keep);
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
void TraceComponent(const std::vector<Cell> &cells, double cell, std::vector<Edge> &edges, int &next_node)
{
    int64_t x0 = INT64_MAX, y0 = INT64_MAX, x1 = INT64_MIN, y1 = INT64_MIN;
    for (Cell c : cells) {
        x0 = std::min(x0, KeyX(c));
        x1 = std::max(x1, KeyX(c));
        y0 = std::min(y0, KeyY(c));
        y1 = std::max(y1, KeyY(c));
    }
    const int w = static_cast<int>(x1 - x0 + 3), h = static_cast<int>(y1 - y0 + 3);
    std::vector<unsigned char> img(static_cast<size_t>(w) * h, 0);
    for (Cell c : cells)
        img[static_cast<size_t>(KeyY(c) - y0 + 1) * w + (KeyX(c) - x0 + 1)] = 1;
    ThinImage(img, w, h);

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
void CleanGraph(std::vector<Edge> &edges, double min_spur)
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
            if ((dead_a != dead_b) && Length(e.pts) < min_spur) {
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

struct Match {
    int line = -1, vertex = -1;
    double offset = 0.0;  // signed distance from the skeleton
};

struct Analysis {
    Proj pr;
    std::vector<std::vector<XY>> xy;          // tracks, metres
    std::vector<std::vector<XY>> lines;       // skeleton lines, one vertex per cell
    std::vector<std::vector<Match>> matches;  // per track point
    std::vector<std::vector<TrackPiece>> pieces;
    std::vector<std::set<size_t>> line_tracks;
    std::vector<bool> kept;                   // shared lines
    std::vector<std::vector<XY>> mean;        // per kept line, per vertex
};

Analysis Analyse(const std::vector<OverviewTrack> &tracks, const SegmentParams &params);

} // namespace

void ThinImage(std::vector<unsigned char> &image, int width, int height)
{
    auto at = [&](int x, int y) -> int {
        return x < 0 || y < 0 || x >= width || y >= height ? 0 : image[static_cast<size_t>(y) * width + x];
    };
    std::vector<size_t> clear;
    for (bool changed = true; changed;) {
        changed = false;
        for (int step = 0; step < 2; ++step) {
            clear.clear();
            for (int y = 0; y < height; ++y)
                for (int x = 0; x < width; ++x) {
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
                    clear.push_back(static_cast<size_t>(y) * width + x);
                }
            for (size_t i : clear)
                image[i] = 0;
            changed = changed || !clear.empty();
        }
    }
}

namespace {

// Everything both outputs need: projection, tracks in metres, skeleton
// lines (densified), per-point matches, pieces, and the mean track of each
// kept (shared) line, one point per skeleton vertex.
Analysis Analyse(const std::vector<OverviewTrack> &tracks, const SegmentParams &params)
{
    Analysis analysis;
    // 1. Projection around the tracks' centre.
    Proj &pr = analysis.pr;
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
    std::vector<std::vector<XY>> &xy = analysis.xy;
    xy.assign(tracks.size(), {});
    for (size_t t = 0; t < tracks.size(); ++t)
        for (size_t i = 0; i < tracks[t].lat.size(); ++i) {
            XY q;
            pr.Fwd(tracks[t].lon[i], tracks[t].lat[i], q.x, q.y);
            xy[t].push_back(q);
        }

    // @impl 0f87-6acd-60a9-7bf6
    // 2. Blurred heat map: 1 on the cells a track's line crosses, 0.5 on
    // their neighbours, once per track (the higher value); tracks add up.
    const double cell = params.cell_m, r = params.tolerance_m;
    std::unordered_map<Cell, double> heat;
    std::unordered_map<Cell, int> reached;  // distinct tracks per cell
    for (const std::vector<XY> &l : xy) {
        std::unordered_map<Cell, double> mine;
        for (const XY &q : Densify(l, cell * 0.5)) {
            const int64_t ix = static_cast<int64_t>(std::floor(q.x / cell));
            const int64_t iy = static_cast<int64_t>(std::floor(q.y / cell));
            mine[Key(ix, iy)] = 1.0;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    double &v = mine[Key(ix + dx, iy + dy)];
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
        if (v >= params.min_density && reached[c] >= params.min_tracks)
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
                    const Cell nb = Key(KeyX(c) + dx, KeyY(c) + dy);
                    if (corridor.count(nb) && seen.insert(nb).second)
                        q.push_back(nb);
                }
        }
        TraceComponent(comp, cell, edges, next_node);
    }
    CleanGraph(edges, params.min_spur_m);
    std::vector<std::vector<XY>> &lines = analysis.lines;  // densified skeleton per kept edge
    for (Edge &ed : edges)
        if (!ed.removed && Length(ed.pts) >= params.min_spur_m * 0.5)
            lines.push_back(Densify(Simplify(ed.pts, cell * 0.5), cell));

    // 5. Map tracks: nearest skeleton vertex within tolerance (grid index).
    const double bucket = std::max(10.0, r * 2.0);
    std::unordered_map<Cell, std::vector<std::pair<int, int>>> index;
    for (int li = 0; li < static_cast<int>(lines.size()); ++li)
        for (int vi = 0; vi < static_cast<int>(lines[static_cast<size_t>(li)].size()); ++vi) {
            const XY &v = lines[static_cast<size_t>(li)][static_cast<size_t>(vi)];
            index[Key(static_cast<int64_t>(std::floor(v.x / bucket)), static_cast<int64_t>(std::floor(v.y / bucket)))]
                .push_back({li, vi});
        }
    const double snap = r * 1.5;
    std::vector<std::vector<Match>> &matches = analysis.matches;
    matches.assign(tracks.size(), {});
    for (size_t t = 0; t < tracks.size(); ++t) {
        for (const XY &q : xy[t]) {
            Match m;
            double best = snap;
            const int64_t bx = static_cast<int64_t>(std::floor(q.x / bucket)), by = static_cast<int64_t>(std::floor(q.y / bucket));
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    auto it = index.find(Key(bx + dx, by + dy));
                    if (it == index.end())
                        continue;
                    for (auto [li, vi] : it->second) {
                        const XY &v = lines[static_cast<size_t>(li)][static_cast<size_t>(vi)];
                        const double d = Dist(v, q);
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
    std::vector<std::set<size_t>> &line_tracks = analysis.line_tracks;
    line_tracks.assign(lines.size(), {});
    std::vector<std::vector<TrackPiece>> &pieces = analysis.pieces;
    pieces.assign(tracks.size(), {});
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
                len += Dist(xy[t][k2 - 1], xy[t][k2]);
            if (pc.segment >= 0 && len < params.min_piece_m)
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

    // Short excursions: a track leaving a line and rejoining the same line
    // within max_excursion_m (GPS drift, a passing place) never left it.
    for (size_t t = 0; t < pieces.size(); ++t) {
        std::vector<TrackPiece> &ps = pieces[t];
        std::vector<TrackPiece> merged;
        for (size_t i = 0; i < ps.size(); ++i) {
            if (ps[i].segment < 0 && !merged.empty() && i + 1 < ps.size() &&
                merged.back().segment >= 0 && ps[i + 1].segment == merged.back().segment) {
                double len = 0.0;
                for (size_t k = ps[i].first; k <= ps[i].last + 1 && k < xy[t].size(); ++k)
                    if (k > 0)
                        len += Dist(xy[t][k - 1], xy[t][k]);
                if (len <= params.max_excursion_m) {
                    merged.back().last = ps[i + 1].last;
                    ++i;  // the rejoined piece is absorbed too
                    continue;
                }
            }
            merged.push_back(ps[i]);
        }
        ps.swap(merged);
    }

    // Shared lines (>= min_tracks routes) and their mean tracks; pieces on
    // other lines become their tracks' own parts.
    analysis.kept.assign(lines.size(), false);
    analysis.mean.assign(lines.size(), {});
    for (size_t li = 0; li < lines.size(); ++li) {
        if (static_cast<int>(line_tracks[li].size()) < params.min_tracks)
            continue;
        analysis.kept[li] = true;
        const std::vector<XY> &l = lines[li];
        // Offset per vertex: mean over tracks of each track's mean offset.
        std::vector<std::map<size_t, std::pair<double, int>>> per(l.size());
        for (size_t t : line_tracks[li])
            for (const TrackPiece &pc : pieces[t])
                if (pc.segment == static_cast<int>(li))
                    for (size_t k2 = pc.first; k2 <= pc.last; ++k2) {
                        const Match &m = matches[t][k2];
                        if (m.line != static_cast<int>(li))
                            continue;  // a bridged excursion point
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
        // Smooth and fill gaps over +/- 4 vertices.
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
            analysis.mean[li].push_back(tl > 0 ? XY{l[v].x - ty / tl * o, l[v].y + tx / tl * o} : l[v]);
        }
    }
    for (std::vector<TrackPiece> &ps : pieces) {
        std::vector<TrackPiece> merged;
        for (TrackPiece pc : ps) {
            if (pc.segment >= 0 && !analysis.kept[static_cast<size_t>(pc.segment)])
                pc.segment = -1;
            if (!merged.empty() && merged.back().segment == pc.segment)
                merged.back().last = pc.last;
            else
                merged.push_back(pc);
        }
        ps.swap(merged);
    }
    for (size_t li = 0; li < lines.size(); ++li)
        if (!analysis.kept[li])
            line_tracks[li].clear();
    return analysis;
}

} // namespace

// @impl 0f87-6acd-6028-c636
SegmentResult FindSegments(const std::vector<OverviewTrack> &tracks, const SegmentParams &params)
{
    SegmentResult out;
    if (tracks.empty())
        return out;
    const Analysis analysis = Analyse(tracks, params);
    std::vector<int> new_id(analysis.lines.size(), -1);
    for (size_t li = 0; li < analysis.lines.size(); ++li) {
        if (!analysis.kept[li])
            continue;
        Segment sg;
        sg.id = static_cast<int>(out.segments.size());
        const std::vector<XY> mean = Simplify(analysis.mean[li], params.cell_m * 0.3);
        sg.length_m = Length(mean);
        for (const XY &q : mean) {
            double lon, lat;
            analysis.pr.Inv(q.x, q.y, lon, lat);
            sg.lon.push_back(lon);
            sg.lat.push_back(lat);
        }
        for (size_t t : analysis.line_tracks[li])
            sg.track_ids.push_back(tracks[t].id);
        new_id[li] = sg.id;
        out.segments.push_back(std::move(sg));
    }
    for (size_t t = 0; t < tracks.size(); ++t) {
        TrackMapping tm;
        tm.id = tracks[t].id;
        for (TrackPiece pc : analysis.pieces[t]) {
            pc.segment = pc.segment >= 0 ? new_id[static_cast<size_t>(pc.segment)] : -1;
            tm.pieces.push_back(pc);
        }
        out.mappings.push_back(std::move(tm));
    }
    return out;
}

// @impl 0f87-6acd-c829-c24f
SegmentNetwork BuildNetwork(const std::vector<OverviewTrack> &tracks, const SegmentParams &params)
{
    SegmentNetwork net;
    if (tracks.empty())
        return net;
    const Analysis analysis = Analyse(tracks, params);
    const std::vector<std::vector<XY>> &lines = analysis.lines;
    // Entry points closer than ~30 m (in skeleton vertices) are one cut.
    const int margin = std::max(2, static_cast<int>(std::ceil(30.0 / params.cell_m)));

    // 1. Traversals: each shared piece as monotone runs along its line (a
    // turn-around starts a new run); own pieces kept as they are.
    struct Trav {
        int line = -1;           // -1: the route's own part
        int from_v = 0, to_v = 0;
        size_t first = 0, last = 0;
    };
    std::vector<std::vector<Trav>> travs(tracks.size());
    std::vector<std::vector<int>> cut_candidates(lines.size());
    for (size_t t = 0; t < tracks.size(); ++t) {
        for (const TrackPiece &pc : analysis.pieces[t]) {
            if (pc.segment < 0) {
                travs[t].push_back({-1, 0, 0, pc.first, pc.last});
                continue;
            }
            const std::vector<Match> &m = analysis.matches[t];
            size_t f0 = pc.first;
            while (f0 < pc.last && m[f0].line != pc.segment)
                ++f0;
            int start_v = m[f0].vertex, ext_v = start_v, dir = 0;
            size_t start_i = pc.first, ext_i = f0;
            for (size_t k = f0 + 1; k <= pc.last; ++k) {
                if (m[k].line != pc.segment)
                    continue;  // a bridged excursion point
                const int v = m[k].vertex;
                if (dir == 0) {
                    if (std::abs(v - start_v) > margin / 2)
                        dir = v > start_v ? 1 : -1;
                    ext_v = v;
                    ext_i = k;
                } else if ((dir > 0 && v >= ext_v) || (dir < 0 && v <= ext_v)) {
                    ext_v = v;
                    ext_i = k;
                } else if (std::abs(v - ext_v) > margin) {  // turned around
                    travs[t].push_back({pc.segment, start_v, ext_v, start_i, ext_i});
                    start_v = ext_v;
                    start_i = ext_i;
                    dir = -dir;
                    ext_v = v;
                    ext_i = k;
                }
            }
            travs[t].push_back({pc.segment, start_v, ext_v, start_i, pc.last});
        }
        for (const Trav &tr : travs[t]) {
            if (tr.line < 0)
                continue;
            const int line_length = static_cast<int>(lines[static_cast<size_t>(tr.line)].size());
            for (int v : {tr.from_v, tr.to_v})
                if (v > margin && v < line_length - 1 - margin)
                    cut_candidates[static_cast<size_t>(tr.line)].push_back(v);
        }
    }

    // 2. Cuts per shared line: line ends plus clustered entry/exit points.
    std::vector<std::vector<int>> cuts(lines.size());
    for (size_t li = 0; li < lines.size(); ++li) {
        if (!analysis.kept[li])
            continue;
        const int line_length = static_cast<int>(lines[li].size());
        std::vector<int> c = cut_candidates[li];
        std::sort(c.begin(), c.end());
        cuts[li].push_back(0);
        for (size_t i = 0; i < c.size();) {
            size_t j = i;
            long sum = c[i];
            while (j + 1 < c.size() && c[j + 1] - c[j] <= margin)
                sum += c[++j];
            const int rep = static_cast<int>(std::lround(static_cast<double>(sum) / static_cast<double>(j - i + 1)));
            if (rep - cuts[li].back() > margin && line_length - 1 - rep > margin)
                cuts[li].push_back(rep);
            i = j + 1;
        }
        cuts[li].push_back(line_length - 1);
    }

    // 3. Nodes. Line ends meeting at a skeleton junction are one node
    // (union by proximity); interior cuts are nodes of their own.
    std::vector<int> parent;
    auto find = [&](int x) {
        while (parent[static_cast<size_t>(x)] != x)
            x = parent[static_cast<size_t>(x)] = parent[static_cast<size_t>(parent[static_cast<size_t>(x)])];
        return x;
    };
    std::vector<std::vector<int>> node_at(lines.size());  // per cut, provisional id
    std::vector<XY> node_pos;
    auto new_node = [&](const XY &q) {
        parent.push_back(static_cast<int>(parent.size()));
        node_pos.push_back(q);
        return static_cast<int>(parent.size() - 1);
    };
    std::vector<std::pair<int, XY>> ends;  // provisional node, skeleton position
    for (size_t li = 0; li < lines.size(); ++li) {
        if (!analysis.kept[li])
            continue;
        for (size_t k = 0; k < cuts[li].size(); ++k) {
            const int id = new_node(analysis.mean[li][static_cast<size_t>(cuts[li][k])]);
            node_at[li].push_back(id);
            if (k == 0)
                ends.push_back({id, lines[li].front()});
            if (k + 1 == cuts[li].size())
                ends.push_back({id, lines[li].back()});
        }
    }
    for (size_t i = 0; i < ends.size(); ++i)
        for (size_t j = i + 1; j < ends.size(); ++j)
            if (Dist(ends[i].second, ends[j].second) <= 2.0 * params.cell_m + 1.0) {
                const int a = find(ends[i].first), b = find(ends[j].first);
                if (a != b)
                    parent[static_cast<size_t>(b)] = a;
            }

    // 4. Shared segments: one per range between consecutive cuts.
    struct Proto {
        int from, to;
        std::vector<XY> pts;
        std::set<size_t> routes;
    };
    std::vector<Proto> protos;
    std::vector<std::vector<int>> proto_of(lines.size());
    for (size_t li = 0; li < lines.size(); ++li) {
        if (!analysis.kept[li])
            continue;
        for (size_t k = 0; k + 1 < cuts[li].size(); ++k) {
            Proto pr;
            pr.from = node_at[li][k];
            pr.to = node_at[li][k + 1];
            pr.pts.assign(analysis.mean[li].begin() + cuts[li][k], analysis.mean[li].begin() + cuts[li][k + 1] + 1);
            proto_of[li].push_back(static_cast<int>(protos.size()));
            protos.push_back(std::move(pr));
        }
    }

    // 5. Routes: shared traversals cover whole ranges (more than half of a
    // range overlapped); own parts become single-route segments joining the
    // nodes where the route leaves and rejoins the shared network.
    struct Item {
        int proto = -1;
        bool forward = true;
        int from = -1, to = -1;  // nodes in travel order
    };
    std::vector<std::vector<Item>> items(tracks.size());
    for (size_t t = 0; t < tracks.size(); ++t) {
        for (const Trav &tr : travs[t]) {
            if (tr.line < 0) {
                // Own part: geometry from the track; nodes filled below.
                Proto own;
                own.from = own.to = -1;
                for (size_t k = tr.first; k <= tr.last; ++k)
                    own.pts.push_back(analysis.xy[t][k]);
                own.routes.insert(t);
                items[t].push_back({static_cast<int>(protos.size()), true, -1, -1});
                protos.push_back(std::move(own));
                continue;
            }
            const size_t li = static_cast<size_t>(tr.line);
            const int lo = std::min(tr.from_v, tr.to_v), hi = std::max(tr.from_v, tr.to_v);
            const bool fwd = tr.to_v >= tr.from_v;
            std::vector<size_t> covered;
            for (size_t k = 0; k + 1 < cuts[li].size(); ++k) {
                const int a = cuts[li][k], b = cuts[li][k + 1];
                const int overlap = std::min(b, hi) - std::max(a, lo);
                if (overlap * 2 > b - a)
                    covered.push_back(k);
            }
            if (!fwd)
                std::reverse(covered.begin(), covered.end());
            for (size_t k : covered) {
                Item it;
                it.proto = proto_of[li][k];
                it.forward = fwd;
                it.from = fwd ? node_at[li][k] : node_at[li][k + 1];
                it.to = fwd ? node_at[li][k + 1] : node_at[li][k];
                protos[static_cast<size_t>(it.proto)].routes.insert(t);
                items[t].push_back(it);
            }
        }
        // Junction crossings: going from one shared segment to another
        // directly, or through a short own stretch, passes one junction: the
        // exit and entry nodes are the same node, and the stretch is no
        // segment of its own.
        std::vector<Item> &its = items[t];
        {
            std::vector<Item> kept_items;
            for (size_t i = 0; i < its.size(); ++i) {
                const Proto &pr = protos[static_cast<size_t>(its[i].proto)];
                const bool own = pr.from == -1 && its[i].from == -1;
                const bool between = !kept_items.empty() && kept_items.back().to >= 0 && i + 1 < its.size() &&
                                     its[i + 1].from >= 0;
                if (own && between && Length(pr.pts) <= params.max_excursion_m &&
                    Dist(node_pos[static_cast<size_t>(kept_items.back().to)],
                         node_pos[static_cast<size_t>(its[i + 1].from)]) <= params.max_excursion_m) {
                    const int a = find(kept_items.back().to), b = find(its[i + 1].from);
                    if (a != b)
                        parent[static_cast<size_t>(b)] = a;
                    protos[static_cast<size_t>(its[i].proto)].routes.erase(t);
                    continue;
                }
                if (!own && !kept_items.empty() && kept_items.back().to >= 0 && its[i].from >= 0 &&
                    Dist(node_pos[static_cast<size_t>(kept_items.back().to)],
                         node_pos[static_cast<size_t>(its[i].from)]) <= params.max_excursion_m) {
                    const int a = find(kept_items.back().to), b = find(its[i].from);
                    if (a != b)
                        parent[static_cast<size_t>(b)] = a;
                }
                kept_items.push_back(its[i]);
            }
            its.swap(kept_items);
        }
        // Own parts connect the previous step's exit to the next step's
        // entry (or a new node at the route's start or end).
        for (size_t i = 0; i < its.size(); ++i) {
            Proto &pr = protos[static_cast<size_t>(its[i].proto)];
            if (pr.from != -1 || pr.routes.size() != 1 || its[i].from != -1)
                continue;
            const int prev = i > 0 ? its[i - 1].to : -1;
            const int next = i + 1 < its.size() ? its[i + 1].from : -1;
            its[i].from = prev >= 0 ? prev : new_node(pr.pts.front());
            its[i].to = next >= 0 ? next : new_node(pr.pts.back());
            pr.from = its[i].from;
            pr.to = its[i].to;
        }
    }

    // 6. Output: final node ids (merged), segments used by a route, routes.
    // analysis node's position is the mean of its provisional members in use.
    std::set<int> used;
    for (const Proto &pr : protos)
        if (!pr.routes.empty() && pr.from >= 0 && pr.to >= 0) {
            used.insert(pr.from);
            used.insert(pr.to);
        }
    std::map<int, int> final_of_root;
    std::vector<XY> sum_pos;
    std::vector<int> sum_n;
    for (int prov : used) {
        const int root = find(prov);
        auto it = final_of_root.find(root);
        if (it == final_of_root.end()) {
            it = final_of_root.emplace(root, static_cast<int>(sum_pos.size())).first;
            sum_pos.push_back({0.0, 0.0});
            sum_n.push_back(0);
        }
        sum_pos[static_cast<size_t>(it->second)].x += node_pos[static_cast<size_t>(prov)].x;
        sum_pos[static_cast<size_t>(it->second)].y += node_pos[static_cast<size_t>(prov)].y;
        ++sum_n[static_cast<size_t>(it->second)];
    }
    auto node_id = [&](int prov) { return final_of_root[find(prov)]; };
    std::vector<int> seg_id(protos.size(), -1);
    for (size_t i = 0; i < protos.size(); ++i) {
        const Proto &pr = protos[i];
        if (pr.routes.empty() || pr.from < 0 || pr.to < 0)
            continue;
        NetworkSegment sg;
        sg.id = static_cast<int>(net.segments.size());
        sg.from = node_id(pr.from);
        sg.to = node_id(pr.to);
        std::vector<XY> pts = Simplify(pr.pts, params.cell_m * 0.3);
        sg.length_m = Length(pts);
        for (const XY &q : pts) {
            double lon, lat;
            analysis.pr.Inv(q.x, q.y, lon, lat);
            sg.lon.push_back(lon);
            sg.lat.push_back(lat);
        }
        for (size_t t : pr.routes)
            sg.route_ids.push_back(tracks[t].id);
        seg_id[i] = sg.id;
        net.segments.push_back(std::move(sg));
    }
    for (size_t n = 0; n < sum_pos.size(); ++n) {
        NetworkNode nd;
        nd.id = static_cast<int>(n);
        analysis.pr.Inv(sum_pos[n].x / sum_n[n], sum_pos[n].y / sum_n[n], nd.lon, nd.lat);
        net.nodes.push_back(nd);
    }
    for (size_t t = 0; t < tracks.size(); ++t) {
        NetworkRoute r;
        r.id = tracks[t].id;
        for (const Item &it : items[t]) {
            const int sid = seg_id[static_cast<size_t>(it.proto)];
            if (sid >= 0)
                r.steps.push_back({sid, it.forward});
        }
        net.routes.push_back(std::move(r));
    }
    return net;
}
} // namespace core
