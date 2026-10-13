/// @file
/// Tile image decoding to 8-bit RGBA: PNG and JPEG with stb_image, TIFF with
/// libtiff. The format is detected from the data, not from a file name
/// (module-level utilities).
#pragma once

#include <openrider/sdk/result.h>

#include <string>
#include <vector>

namespace maptiles {

/// A decoded image.
struct Image {
    int width = 0;                    ///< pixels
    int height = 0;                   ///< pixels
    std::vector<unsigned char> rgba;  ///< width * height * 4, row-major from the top
};

/// Decodes a tile file (PNG, JPEG or TIFF) to RGBA.
/// @param bytes the file's content
openrider::sdk::Result<Image> DecodeTile(const std::string &bytes);

} // namespace maptiles
