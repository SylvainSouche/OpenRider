// CPU raster warping: pixels land where the transformation sends them.

#include <atf-c++.hpp>

#include "reproj/raster_warp.h"
#include "shipped.h"

#include <cmath>

namespace {

// A 256x256 image, black but for one white square [96, 160).
reproj::Image Square()
{
    reproj::Image image;
    image.width = image.height = 256;
    image.rgba.assign(256 * 256 * 4, 0);
    for (int row = 0; row < 256; ++row)
        for (int column = 0; column < 256; ++column) {
            uint8_t *pixel = &image.rgba[static_cast<size_t>((row * 256 + column) * 4)];
            const bool white = row >= 96 && row < 160 && column >= 96 && column < 160;
            pixel[0] = pixel[1] = pixel[2] = white ? 255 : 0;
            pixel[3] = 255;
        }
    return image;
}

} // namespace

ATF_TEST_CASE_WITHOUT_HEAD(identity_reproduces_the_image);
ATF_TEST_CASE_BODY(identity_reproduces_the_image)
{
    const reproj::Transformer transformer(ShippedCrs(3857), ShippedCrs(3857));
    const reproj::Image source = Square();
    const reproj::Box extent{0.0, 2560.0, 2560.0, 0.0};
    reproj::Image target;
    target.width = target.height = 256;
    reproj::WarpRaster(transformer, source, extent, extent, target);
    ATF_REQUIRE(target.rgba == source.rgba);
}

ATF_TEST_CASE_WITHOUT_HEAD(square_lands_where_projected);
ATF_TEST_CASE_BODY(square_lands_where_projected)
{
    // A Web Mercator raster near Svolvær warped into UTM 33N: the white
    // square's centre must land at the UTM position of its own centre.
    const reproj::Crs mercator = ShippedCrs(3857), utm = ShippedCrs(25833);
    const reproj::Image source = Square();
    const reproj::Box source_extent{1615000.0, 10525000.0, 1628000.0, 10512000.0};  // 13 km square
    // The target window: the source's corners in UTM, enlarged a little.
    const reproj::Transformer forward(mercator, utm);
    double x0, y0, x1, y1;
    ATF_REQUIRE(forward.Transform(source_extent.x0, source_extent.y0, x0, y0));
    ATF_REQUIRE(forward.Transform(source_extent.x1, source_extent.y1, x1, y1));
    const reproj::Box target_extent{x0 - 500.0, y0 + 500.0, x1 + 500.0, y1 - 500.0};
    reproj::Image target;
    target.width = target.height = 300;
    reproj::WarpRaster(forward.Inverse(), source, source_extent, target_extent, target);
    // White pixels' centroid, in UTM metres.
    double sum_x = 0.0, sum_y = 0.0;
    int white = 0;
    for (int row = 0; row < target.height; ++row)
        for (int column = 0; column < target.width; ++column)
            if (target.rgba[static_cast<size_t>((row * target.width + column) * 4)] > 127) {
                sum_x += target_extent.x0 + (target_extent.x1 - target_extent.x0) * (column + 0.5) / target.width;
                sum_y += target_extent.y0 + (target_extent.y1 - target_extent.y0) * (row + 0.5) / target.height;
                ++white;
            }
    ATF_REQUIRE(white > 0);
    double cx, cy;
    ATF_REQUIRE(forward.Transform((source_extent.x0 + source_extent.x1) / 2, (source_extent.y0 + source_extent.y1) / 2, cx, cy));
    const double target_pixel = (target_extent.x1 - target_extent.x0) / target.width;
    ATF_REQUIRE(std::hypot(sum_x / white - cx, sum_y / white - cy) < target_pixel);
    // Outside the source's footprint the target stays transparent.
    ATF_REQUIRE_EQ(target.rgba[3], 0);
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, identity_reproduces_the_image);
    ATF_ADD_TEST_CASE(tcs, square_lands_where_projected);
}
