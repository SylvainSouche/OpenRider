#include "reproj/warp_mesh.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace reproj {

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// Nodes of an n x n cell grid over `extent`, transformed (NaN on failure).
std::vector<MeshVertex> Grid(const Transformer &transformer, const Box &extent, int n)
{
    std::vector<MeshVertex> nodes(static_cast<size_t>((n + 1) * (n + 1)));
    for (int row = 0; row <= n; ++row)
        for (int column = 0; column <= n; ++column) {
            MeshVertex &node = nodes[static_cast<size_t>(row * (n + 1) + column)];
            node.u = static_cast<float>(column) / n;
            node.v = static_cast<float>(row) / n;
            const double x = extent.x0 + (extent.x1 - extent.x0) * column / n;
            const double y = extent.y0 + (extent.y1 - extent.y0) * row / n;
            if (!transformer.Transform(x, y, node.x, node.y))
                node.x = node.y = kNaN;
        }
    return nodes;
}

// Distance between a node and the midpoint of two others.
double MidpointError(const MeshVertex &exact, const MeshVertex &a, const MeshVertex &b)
{
    return std::hypot(exact.x - (a.x + b.x) / 2.0, exact.y - (a.y + b.y) / 2.0);
}

} // namespace

WarpMesh BuildWarpMesh(const Transformer &transformer, const Box &extent, double tolerance, int max_cells)
{
    WarpMesh mesh;
    // The grid of n cells and the twice finer one: the finer one's extra
    // nodes are the exact positions of the coarse cells' midpoints. When the
    // coarse grid is not good enough, the finer one becomes the next coarse.
    int n = 1;
    std::vector<MeshVertex> coarse = Grid(transformer, extent, n);
    for (;;) {
        std::vector<MeshVertex> fine = Grid(transformer, extent, 2 * n);
        const int fine_side = 2 * n + 1;
        auto coarse_at = [&](int row, int column) -> const MeshVertex & {
            return coarse[static_cast<size_t>(row * (n + 1) + column)];
        };
        auto fine_at = [&](int row, int column) -> const MeshVertex & {
            return fine[static_cast<size_t>(row * fine_side + column)];
        };
        double error = 0.0;
        for (int row = 0; row < n; ++row)
            for (int column = 0; column < n; ++column) {
                const MeshVertex &top_left = coarse_at(row, column), &top_right = coarse_at(row, column + 1);
                const MeshVertex &bottom_left = coarse_at(row + 1, column), &bottom_right = coarse_at(row + 1, column + 1);
                const MeshVertex &centre = fine_at(2 * row + 1, 2 * column + 1);
                const double errors[] = {
                    MidpointError(fine_at(2 * row, 2 * column + 1), top_left, top_right),
                    MidpointError(fine_at(2 * row + 2, 2 * column + 1), bottom_left, bottom_right),
                    MidpointError(fine_at(2 * row + 1, 2 * column), top_left, bottom_left),
                    MidpointError(fine_at(2 * row + 1, 2 * column + 2), top_right, bottom_right),
                    MidpointError(centre, top_left, bottom_right),  // the triangles' shared diagonal
                    std::hypot(centre.x - (top_left.x + top_right.x + bottom_left.x + bottom_right.x) / 4.0,
                               centre.y - (top_left.y + top_right.y + bottom_left.y + bottom_right.y) / 4.0),
                };
                for (double e : errors)
                    if (std::isfinite(e))
                        error = std::max(error, e);
                // A cell straddling the edge of the transformation's domain
                // -- some of its nine nodes at this and the finer level
                // transformed, some not, even all four corners failing
                // around a valid middle: refine, so that the drawn part
                // hugs that edge.
                int valid = 0;
                for (int r = 2 * row; r <= 2 * row + 2; ++r)
                    for (int c = 2 * column; c <= 2 * column + 2; ++c)
                        valid += std::isfinite(fine_at(r, c).x) ? 1 : 0;
                if (valid > 0 && valid < 9)
                    error = std::numeric_limits<double>::infinity();
            }
        if (error <= tolerance || 2 * n > max_cells) {
            mesh.max_error = std::isfinite(error) ? error : 0.0;
            break;
        }
        n *= 2;
        coarse = std::move(fine);
    }
    mesh.cells = n;
    mesh.vertices = std::move(coarse);
    const uint32_t side = static_cast<uint32_t>(n + 1);
    for (uint32_t row = 0; row < static_cast<uint32_t>(n); ++row)
        for (uint32_t column = 0; column < static_cast<uint32_t>(n); ++column) {
            const uint32_t top_left = row * side + column, top_right = top_left + 1;
            const uint32_t bottom_left = top_left + side, bottom_right = bottom_left + 1;
            bool complete = true;
            for (uint32_t k : {top_left, top_right, bottom_left, bottom_right})
                complete = complete && std::isfinite(mesh.vertices[k].x);
            if (!complete)
                continue;
            mesh.indices.insert(mesh.indices.end(), {top_left, top_right, bottom_right, top_left, bottom_right, bottom_left});
        }
    return mesh;
}

} // namespace reproj
