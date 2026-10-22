#include "reproj/transformer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <thread>
#include <utility>
#include <vector>

namespace reproj {

namespace {

// Large batches are split across the CPU's cores, each taking at least
// this many vertices (one costs a few hundred nanoseconds).
constexpr size_t kMinShare = 32768;

} // namespace

Transformer::Transformer(Crs source, Crs target)
    : source_(std::move(source)), target_(std::move(target)),
      identity_(source_.epsg != 0 && source_.epsg == target_.epsg && source_.definition == target_.definition)
{
}

bool Transformer::Transform(double x, double y, double &out_x, double &out_y) const
{
    if (identity_) {
        out_x = x;
        out_y = y;
        return std::isfinite(x) && std::isfinite(y);
    }
    // Source projection -> source datum -> WGS 84 -> target datum -> target
    // projection; the datum steps do nothing between WGS 84-like datums.
    double lon, lat;
    if (!source_.projection->Inverse(x, y, lon, lat))
        return false;
    ToWgs84(source_.datum, lon, lat);
    FromWgs84(target_.datum, lon, lat);
    return target_.projection->Forward(lon, lat, out_x, out_y);
}

size_t Transformer::TransformPoints(double *xs, double *ys, size_t count) const
{
    const size_t cores = std::max(1u, std::thread::hardware_concurrency());
    const size_t workers = std::min(cores, count / kMinShare);
    if (workers <= 1)
        return TransformRange(xs, ys, count);
    // One share per worker; each counts its own failures.
    const size_t share = (count + workers - 1) / workers;
    std::vector<size_t> failed(workers, 0);
    std::vector<std::thread> threads;
    for (size_t w = 0; w < workers; ++w) {
        const size_t first = w * share, n = std::min(share, count - first);
        threads.emplace_back([this, xs, ys, first, n, w, &failed] { failed[w] = TransformRange(xs + first, ys + first, n); });
    }
    for (std::thread &thread : threads)
        thread.join();
    size_t total = 0;
    for (size_t f : failed)
        total += f;
    return total;
}

size_t Transformer::TransformRange(double *xs, double *ys, size_t count) const
{
    constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
    size_t failed = 0;
    for (size_t i = 0; i < count; ++i) {
        double x, y;
        if (Transform(xs[i], ys[i], x, y)) {
            xs[i] = x;
            ys[i] = y;
        } else {
            xs[i] = ys[i] = kNaN;
            ++failed;
        }
    }
    return failed;
}

} // namespace reproj
