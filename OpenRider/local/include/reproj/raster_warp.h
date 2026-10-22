/// @file
/// Raster warping on the CPU: the fallback of the mesh path, for exports
/// and tests. Same interpolation grid as the GPU's, bilinear sampling.
#pragma once

#include "reproj/crs.h"
#include "reproj/transformer.h"

#include <cstdint>
#include <vector>

namespace reproj {

/// An RGBA image, 8 bits per channel, rows from the top.
struct Image {
    int width = 0;                 ///< pixels
    int height = 0;                ///< pixels
    std::vector<uint8_t> rgba;     ///< width * height * 4 bytes
};

/// Warps `source`, covering `source_extent` (top-left (x0, y0), bottom-right
/// (x1, y1)) in the source system of `target_to_source`, into `target`,
/// covering `target_extent` in its target system; `target` keeps its size
/// and is cleared first. Each target pixel centre is mapped to the source
/// through a grid refined until it errs by at most `tolerance_px` source
/// pixels, then sampled bilinearly; pixels mapping outside the source stay
/// transparent. Rows are spread over the CPU's cores.
void WarpRaster(const Transformer &target_to_source, const Image &source, const Box &source_extent,
                const Box &target_extent, Image &target, double tolerance_px = 0.125);

} // namespace reproj
