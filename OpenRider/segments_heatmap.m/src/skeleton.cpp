#include "heatmap/skeleton.h"

#include "heatmap/heatmap.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace heatmap {

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

} // namespace heatmap
