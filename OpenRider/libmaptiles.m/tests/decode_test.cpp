#include <atf-c++.hpp>

#include "maptiles/decode.h"

#include <tiffio.h>

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

// 2x1 RGBA PNG: a red pixel, then a blue one.
const char kPng[] =
    "\x89\x50\x4e\x47\x0d\x0a\x1a\x0a\x00\x00\x00\x0d\x49\x48\x44\x52\x00\x00\x00\x02\x00\x00"
    "\x00\x01\x08\x06\x00\x00\x00\xf4\x22\x7f\x8a\x00\x00\x00\x0e\x49\x44\x41\x54\x78\x9c\x63"
    "\xf8\xcf\xc0\x00\x42\xff\x01\x0f\xf9\x03\xfd\x85\x11\x99\x76\x00\x00\x00\x00\x49\x45\x4e"
    "\x44\xae\x42\x60\x82";

std::string tiff_bytes()
{
    char path[] = "/tmp/openrider-tiff-XXXXXX";
    const int fd = mkstemp(path);
    std::fclose(fdopen(fd, "w"));
    TIFF *t = TIFFOpen(path, "w");
    const unsigned char row[] = {0, 255, 0, 255, 0, 255};  // 3 green RGB pixels
    TIFFSetField(t, TIFFTAG_IMAGEWIDTH, 3);
    TIFFSetField(t, TIFFTAG_IMAGELENGTH, 1);
    TIFFSetField(t, TIFFTAG_SAMPLESPERPIXEL, 3);
    TIFFSetField(t, TIFFTAG_BITSPERSAMPLE, 8);
    TIFFSetField(t, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
    TIFFSetField(t, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFWriteScanline(t, const_cast<unsigned char *>(row), 0, 0);
    TIFFClose(t);
    std::string out;
    FILE *f = std::fopen(path, "rb");
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0)
        out.append(buf, n);
    std::fclose(f);
    std::remove(path);
    return out;
}

} // namespace

ATF_TEST_CASE_WITHOUT_HEAD(decodes_png);
ATF_TEST_CASE_BODY(decodes_png)
{
    const auto r = maptiles::decode_tile(std::string(kPng, sizeof kPng - 1));
    ATF_REQUIRE(r.ok());
    ATF_REQUIRE_EQ(r.value.width, 2);
    ATF_REQUIRE_EQ(r.value.height, 1);
    ATF_REQUIRE_EQ(r.value.rgba[0], 255);  // red
    ATF_REQUIRE_EQ(r.value.rgba[6], 255);  // blue channel of the second pixel
}

ATF_TEST_CASE_WITHOUT_HEAD(decodes_tiff);
ATF_TEST_CASE_BODY(decodes_tiff)
{
    const auto r = maptiles::decode_tile(tiff_bytes());
    ATF_REQUIRE(r.ok());
    ATF_REQUIRE_EQ(r.value.width, 3);
    ATF_REQUIRE_EQ(r.value.rgba[1], 255);  // green
    ATF_REQUIRE_EQ(r.value.rgba[3], 255);  // opaque
}

ATF_TEST_CASE_WITHOUT_HEAD(rejects_garbage);
ATF_TEST_CASE_BODY(rejects_garbage)
{
    ATF_REQUIRE(!maptiles::decode_tile("").ok());
    ATF_REQUIRE(!maptiles::decode_tile("<html>rate limited</html>").ok());
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, decodes_png);
    ATF_ADD_TEST_CASE(tcs, decodes_tiff);
    ATF_ADD_TEST_CASE(tcs, rejects_garbage);
}
