#include "reproj/raster_warp.h"

#include "reproj/warp_mesh.h"

#include <algorithm>
#include <cmath>
#include <thread>

namespace reproj {

namespace {

// Bilinear sample of `image` at pixel coordinates (px, py) -- pixel centres
// at half-integers -- into `out`; false outside the image.
bool Sample(const Image &image, double px, double py, uint8_t out[4])
{
    if (!(px >= 0.0 && py >= 0.0 && px <= image.width && py <= image.height))
        return false;
    const double x = std::clamp(px - 0.5, 0.0, image.width - 1.0), y = std::clamp(py - 0.5, 0.0, image.height - 1.0);
    const int x0 = static_cast<int>(x), y0 = static_cast<int>(y);
    const int x1 = std::min(x0 + 1, image.width - 1), y1 = std::min(y0 + 1, image.height - 1);
    const double fx = x - x0, fy = y - y0;
    auto at = [&](int column, int row, int channel) {
        return static_cast<double>(image.rgba[static_cast<size_t>((row * image.width + column) * 4 + channel)]);
    };
    for (int channel = 0; channel < 4; ++channel) {
        const double top = at(x0, y0, channel) * (1.0 - fx) + at(x1, y0, channel) * fx;
        const double bottom = at(x0, y1, channel) * (1.0 - fx) + at(x1, y1, channel) * fx;
        out[channel] = static_cast<uint8_t>(std::lround(top * (1.0 - fy) + bottom * fy));
    }
    return true;
}

} // namespace

void WarpRaster(const Transformer &target_to_source, const Image &source, const Box &source_extent,
                const Box &target_extent, Image &target, double tolerance_px)
{
    target.rgba.assign(static_cast<size_t>(target.width) * static_cast<size_t>(target.height) * 4, 0);
    if (source.width <= 0 || source.height <= 0 || target.width <= 0 || target.height <= 0)
        return;
    // The grid maps target positions to source positions; its tolerance is
    // in source units.
    const double source_pixel = std::min(std::fabs(source_extent.x1 - source_extent.x0) / source.width,
                                         std::fabs(source_extent.y1 - source_extent.y0) / source.height);
    const WarpMesh grid = BuildWarpMesh(target_to_source, target_extent, tolerance_px * source_pixel, 256);
    const int n = grid.cells;
    auto node = [&](int row, int column) -> const MeshVertex & {
        return grid.vertices[static_cast<size_t>(row * (n + 1) + column)];
    };
    auto warp_rows = [&](int first_row, int last_row) {
        for (int row = first_row; row < last_row; ++row) {
            const double v = (row + 0.5) / target.height * n;
            const int cell_row = std::min(n - 1, static_cast<int>(v));
            const double fy = v - cell_row;
            for (int column = 0; column < target.width; ++column) {
                const double u = (column + 0.5) / target.width * n;
                const int cell_column = std::min(n - 1, static_cast<int>(u));
                const double fx = u - cell_column;
                const MeshVertex &a = node(cell_row, cell_column), &b = node(cell_row, cell_column + 1);
                const MeshVertex &c = node(cell_row + 1, cell_column), &d = node(cell_row + 1, cell_column + 1);
                double sx = (a.x * (1 - fx) + b.x * fx) * (1 - fy) + (c.x * (1 - fx) + d.x * fx) * fy;
                double sy = (a.y * (1 - fx) + b.y * fx) * (1 - fy) + (c.y * (1 - fx) + d.y * fx) * fy;
                if (!std::isfinite(sx) || !std::isfinite(sy)) {
                    // A cell at the domain's edge: this pixel exactly.
                    const double tx = target_extent.x0 + (target_extent.x1 - target_extent.x0) * (column + 0.5) / target.width;
                    const double ty = target_extent.y0 + (target_extent.y1 - target_extent.y0) * (row + 0.5) / target.height;
                    if (!target_to_source.Transform(tx, ty, sx, sy))
                        continue;
                }
                const double px = (sx - source_extent.x0) / (source_extent.x1 - source_extent.x0) * source.width;
                const double py = (sy - source_extent.y0) / (source_extent.y1 - source_extent.y0) * source.height;
                Sample(source, px, py, &target.rgba[static_cast<size_t>((row * target.width + column) * 4)]);
            }
        }
    };
    const int workers = std::clamp(static_cast<int>(std::thread::hardware_concurrency()), 1, std::max(1, target.height / 64));
    if (workers == 1) {
        warp_rows(0, target.height);
        return;
    }
    std::vector<std::thread> threads;
    const int share = (target.height + workers - 1) / workers;
    for (int w = 0; w < workers; ++w)
        threads.emplace_back(warp_rows, w * share, std::min(target.height, (w + 1) * share));
    for (std::thread &thread : threads)
        thread.join();
}

} // namespace reproj
