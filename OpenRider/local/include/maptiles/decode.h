// Tile image decoding to 8-bit RGBA: PNG and JPEG with stb_image, TIFF with
// libtiff. The format is detected from the data, not from a file name.
#pragma once

#include "core/result.h"

#include <string>
#include <vector>

namespace maptiles {

struct Image {
    int width = 0;
    int height = 0;
    std::vector<unsigned char> rgba;  // width * height * 4
};

core::Result<Image> decode_tile(const std::string &bytes);

} // namespace maptiles
