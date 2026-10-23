// Performance of the hot paths, against budgets in calibration units.
//
// The calibration is a fixed single-threaded floating-point workload; each
// measurement is divided by its time, and the ratio must stay within the
// budget. Budgets are three to ten times the ratios measured when they
// were set (Apple M-series, 10 cores, 2026-10-22) -- more where work is
// spread over the cores, as a guest may have two -- so only a gross
// regression fails.
// Every figure is printed: `kyua report --verbose` shows them.

#include <atf-c++.hpp>

#include "app/map_geometry.h"
#include "reproj/crs_registry.h"
#include "reproj/grid_projector.h"
#include "reproj/raster_warp.h"
#include "reproj/transformer.h"
#include "reproj/warp_mesh.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace {

// Wall time of `work`, the best of `runs` (milliseconds).
double Best(int runs, const std::function<void()> &work)
{
    double best = 1e300;
    for (int r = 0; r < runs; ++r) {
        const auto start = std::chrono::steady_clock::now();
        work();
        best = std::min(best, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
    }
    return best;
}

volatile double g_sink = 0.0;  // keeps results alive

// The calibration workload: 2 million rounds of trigonometry (milliseconds).
double Calibration()
{
    static const double time = Best(5, [] {
        double sum = 0.0;
        for (int i = 0; i < 2000000; ++i) {
            const double x = i * 1e-6;
            sum += std::sin(x) * std::cos(x * 0.5) + std::sqrt(x + 1.0);
        }
        g_sink = sum;
    });
    return time;
}

const reproj::CrsRegistry &Registry()
{
    static const reproj::CrsRegistry registry = [] {
        reproj::CrsRegistry loaded;
        loaded.LoadFile(PERF_CRS_FILE);
        return loaded;
    }();
    return registry;
}

// Reports `name`'s time and checks it against `budget` calibration units.
void Check(const std::string &name, double milliseconds, double budget, const std::string &detail)
{
    const double ratio = milliseconds / Calibration();
    std::printf("perf %-28s %9.3f ms  %6.2f units (budget %5.1f)  %s\n", name.c_str(), milliseconds, ratio, budget,
                detail.c_str());
    if (ratio > budget)
        ATF_FAIL(name + ": " + std::to_string(ratio) + " calibration units, budget " + std::to_string(budget));
}

} // namespace

ATF_TEST_CASE_WITHOUT_HEAD(calibration);
ATF_TEST_CASE_BODY(calibration)
{
    std::printf("perf calibration                 %9.3f ms\n", Calibration());
    ATF_REQUIRE(Calibration() > 0.0);
}

ATF_TEST_CASE_WITHOUT_HEAD(transform_points);
ATF_TEST_CASE_BODY(transform_points)
{
    // A million WGS 84 points into UTM 33N, Lambert-93 and RD New (datum
    // shift), in batches (spread over the cores).
    constexpr size_t kCount = 1000000;
    for (int code : {25833, 2154, 28992}) {
        const reproj::Transformer transformer(Registry().Find(4326), Registry().Find(code));
        std::vector<double> xs(kCount), ys(kCount);
        const double time = Best(3, [&] {
            for (size_t i = 0; i < kCount; ++i) {
                xs[i] = 4.0 + 12.0 * static_cast<double>(i) / kCount;
                ys[i] = 46.0 + 20.0 * static_cast<double>(i % 1000) / 1000;
            }
            transformer.TransformPoints(xs.data(), ys.data(), kCount);
        });
        // Measured: 1.6 (Lambert-93), 3.4 (UTM), 9.2 (RD New, datum shift).
        Check("transform 1M -> EPSG:" + std::to_string(code), time, code == 28992 ? 60.0 : 30.0,
              std::to_string(time * 1e6 / kCount).substr(0, 6) + " ns a point");
    }
}

ATF_TEST_CASE_WITHOUT_HEAD(track_drawing);
ATF_TEST_CASE_BODY(track_drawing)
{
    // A frame over the Lofoten in UTM 33N: geometry, then a million track
    // points to screen.
    app::MapCamera camera;
    camera.lon = 14.57;
    camera.lat = 68.23;
    camera.zoom = 12.0;
    constexpr int kCount = 1000000;
    double sum = 0.0;
    const double geometry_time = Best(5, [&] {
        const app::MapGeometry geometry(Registry().Find(25833), camera, 1600, 1000);
        double x, y;
        geometry.ToScreen(14.57, 68.23, x, y);  // builds the grid
        sum += x;
    });
    Check("frame geometry", geometry_time, 1.0, "grid built");  // measured 0.03
    const app::MapGeometry geometry(Registry().Find(25833), camera, 1600, 1000);
    const double time = Best(3, [&] {
        double x, y;
        for (int i = 0; i < kCount; ++i) {
            geometry.ToScreen(14.4 + (i % 1000) * 3e-4, 68.15 + (i % 997) * 1.6e-4, x, y);
            sum += x;
        }
    });
    g_sink = sum;
    // Measured 0.9.
    Check("track points 1M to screen", time, 4.0, std::to_string(time * 1e6 / kCount).substr(0, 5) + " ns a point");
}

ATF_TEST_CASE_WITHOUT_HEAD(tile_warping);
ATF_TEST_CASE_BODY(tile_warping)
{
    // 300 Web Mercator tiles (a full view's worth) meshed into UTM 33N, to a
    // quarter of a 10 m pixel; then one 1024 x 1024 raster on the CPU.
    const reproj::Transformer transformer(Registry().Find(3857), Registry().Find(25833));
    const double world = 2.0 * 3.14159265358979323846 * 6378137.0, size = world / 4096;
    size_t triangles = 0;
    const double meshes = Best(3, [&] {
        triangles = 0;
        for (int column = 2200; column < 2220; ++column)
            for (int row = 1025; row < 1040; ++row) {
                const reproj::Box tile{-world / 2 + column * size, world / 2 - row * size, -world / 2 + (column + 1) * size,
                                       world / 2 - (row + 1) * size};
                triangles += reproj::BuildWarpMesh(transformer, tile, 2.5, 32).indices.size() / 3;
            }
    });
    // Measured 0.1.
    Check("warp meshes, 300 tiles", meshes, 1.0, std::to_string(triangles) + " triangles");
    reproj::Image source;
    source.width = source.height = 1024;
    source.rgba.assign(1024 * 1024 * 4, 128);
    reproj::Image target;
    target.width = target.height = 1024;
    const reproj::Box source_extent{1615000.0, 10525000.0, 1628000.0, 10512000.0};
    // 3 km of UTM centred on Svolvær (482140 E, 7569083 N), inside the
    // source's 13 km of Web Mercator (4.9 km on the ground at 68 N, Svolvær
    // 1.9 km from its southern edge): every pixel is sampled.
    const reproj::Box target_extent{480640.0, 7570583.0, 483640.0, 7567583.0};
    const double raster = Best(3, [&] {
        reproj::WarpRaster(transformer.Inverse(), source, source_extent, target_extent, target);
    });
    size_t filled = 0;
    for (size_t i = 3; i < target.rgba.size(); i += 4)
        filled += target.rgba[i] != 0 ? 1 : 0;
    ATF_REQUIRE_EQ(filled, static_cast<size_t>(1024 * 1024));
    Check("CPU raster warp 1024x1024", raster, 4.0, "bilinear, every pixel");
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, calibration);
    ATF_ADD_TEST_CASE(tcs, transform_points);
    ATF_ADD_TEST_CASE(tcs, track_drawing);
    ATF_ADD_TEST_CASE(tcs, tile_warping);
}
