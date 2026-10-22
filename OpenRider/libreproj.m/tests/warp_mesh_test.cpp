// Warp meshes: refined until within tolerance, complete cells only.

#include <atf-c++.hpp>

#include "reproj/warp_mesh.h"
#include "shipped.h"

#include <cmath>

namespace {

// Extent of Web Mercator tile z/x/y: top-left, bottom-right corners.
reproj::Box TileExtent(int z, int x, int y)
{
    const double world = 2.0 * 3.14159265358979323846 * 6378137.0, size = world / (1 << z);
    return {-world / 2 + x * size, world / 2 - y * size, -world / 2 + (x + 1) * size, world / 2 - (y + 1) * size};
}

} // namespace

ATF_TEST_CASE_WITHOUT_HEAD(tile_into_utm_within_tolerance);
ATF_TEST_CASE_BODY(tile_into_utm_within_tolerance)
{
    // An OpenTopoMap tile over the Lofoten (z12), drawn in UTM 33N: the
    // tolerance is a quarter of a 10 m pixel.
    const reproj::Transformer transformer(ShippedCrs(3857), ShippedCrs(25833));
    const reproj::Box tile = TileExtent(12, 2213, 1033);
    const reproj::WarpMesh mesh = reproj::BuildWarpMesh(transformer, tile, 2.5);
    ATF_REQUIRE(mesh.cells >= 1 && mesh.cells <= 64);
    ATF_REQUIRE(mesh.max_error <= 2.5);
    ATF_REQUIRE_EQ(mesh.vertices.size(), static_cast<size_t>((mesh.cells + 1) * (mesh.cells + 1)));
    ATF_REQUIRE_EQ(mesh.indices.size(), static_cast<size_t>(mesh.cells * mesh.cells * 6));
    // Corners carry the texture's corners and the exact transformation.
    const reproj::MeshVertex &last = mesh.vertices.back();
    ATF_REQUIRE_EQ(last.u, 1.0f);
    ATF_REQUIRE_EQ(last.v, 1.0f);
    double x, y;
    ATF_REQUIRE(transformer.Transform(tile.x1, tile.y1, x, y));
    ATF_REQUIRE_EQ(last.x, x);
    ATF_REQUIRE_EQ(last.y, y);
    // Checked independently: the mesh's linear interpolation at random
    // points stays within the tolerance (plus a margin for interior points
    // not among those the refinement checked).
    for (int k = 0; k < 200; ++k) {
        const double u = (k * 37 % 200) / 200.0 + 0.0025, v = (k * 91 % 200) / 200.0 + 0.0025;
        const double cu = u * mesh.cells, cv = v * mesh.cells;
        const int column = static_cast<int>(cu), row = static_cast<int>(cv);
        const double fx = cu - column, fy = cv - row;
        auto node = [&](int r, int c) -> const reproj::MeshVertex & { return mesh.vertices[r * (mesh.cells + 1) + c]; };
        // Lower-left or upper-right triangle of the cell, as drawn.
        const reproj::MeshVertex &a = node(row, column), &d = node(row + 1, column + 1);
        const reproj::MeshVertex &other = fx >= fy ? node(row, column + 1) : node(row + 1, column);
        const double wa = fx >= fy ? 1.0 - fx : 1.0 - fy, wd = fx >= fy ? fy : fx, wo = 1.0 - wa - wd;
        const double ix = a.x * wa + d.x * wd + other.x * wo, iy = a.y * wa + d.y * wd + other.y * wo;
        double ex, ey;
        ATF_REQUIRE(transformer.Transform(tile.x0 + (tile.x1 - tile.x0) * u, tile.y0 + (tile.y1 - tile.y0) * v, ex, ey));
        ATF_REQUIRE(std::hypot(ix - ex, iy - ey) <= 2.5 * 1.5);
    }
}

ATF_TEST_CASE_WITHOUT_HEAD(identity_is_one_cell);
ATF_TEST_CASE_BODY(identity_is_one_cell)
{
    const reproj::Transformer transformer(ShippedCrs(3857), ShippedCrs(3857));
    const reproj::WarpMesh mesh = reproj::BuildWarpMesh(transformer, TileExtent(5, 16, 11), 0.01);
    ATF_REQUIRE_EQ(mesh.cells, 1);
    ATF_REQUIRE_EQ(mesh.indices.size(), 6u);
}

ATF_TEST_CASE_WITHOUT_HEAD(cells_outside_the_domain_are_dropped);
ATF_TEST_CASE_BODY(cells_outside_the_domain_are_dropped)
{
    // A geographic box from 30W to 90E, drawn in UTM 33N (central meridian
    // 15E): beyond 60 degrees from it, nodes fail and their cells go.
    const reproj::Transformer transformer(ShippedCrs(4326), ShippedCrs(25833));
    const reproj::WarpMesh mesh = reproj::BuildWarpMesh(transformer, {-30.0, 70.0, 90.0, 40.0}, 1000.0, 16);
    ATF_REQUIRE_EQ(mesh.cells, 16);  // refined towards the domain's edge
    ATF_REQUIRE(!mesh.indices.empty());
    ATF_REQUIRE(mesh.indices.size() < static_cast<size_t>(16 * 16 * 6));
    for (uint32_t index : mesh.indices)
        ATF_REQUIRE(std::isfinite(mesh.vertices[index].x));
}

ATF_TEST_CASE_WITHOUT_HEAD(valid_middle_between_failing_corners);
ATF_TEST_CASE_BODY(valid_middle_between_failing_corners)
{
    // From 70W to 100E around UTM 33N's central meridian (15E): every
    // corner lies beyond its 60 degrees, the middle does not.
    const reproj::Transformer transformer(ShippedCrs(4326), ShippedCrs(25833));
    const reproj::WarpMesh mesh = reproj::BuildWarpMesh(transformer, {-70.0, 75.0, 100.0, 45.0}, 1000.0, 16);
    ATF_REQUIRE(!mesh.indices.empty());
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, valid_middle_between_failing_corners);
    ATF_ADD_TEST_CASE(tcs, tile_into_utm_within_tolerance);
    ATF_ADD_TEST_CASE(tcs, identity_is_one_cell);
    ATF_ADD_TEST_CASE(tcs, cells_outside_the_domain_are_dropped);
}
