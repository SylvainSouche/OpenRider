#include "maptiles/decode.h"

#include <stb_image.h>
#include <tiffio.h>

#include <cstring>

namespace maptiles {

namespace {

bool IsTiff(const std::string &b)
{
    return b.size() > 4 && (std::memcmp(b.data(), "II*\0", 4) == 0 ||
                            std::memcmp(b.data(), "MM\0*", 4) == 0);
}

// libtiff reads from memory through these client callbacks.
struct MemStream {
    const std::string *data;
    toff_t pos;
};

tsize_t MemRead(thandle_t h, tdata_t buf, tsize_t size)
{
    MemStream *m = static_cast<MemStream *>(h);
    const toff_t left = m->data->size() > m->pos ? m->data->size() - m->pos : 0;
    const tsize_t n = static_cast<tsize_t>(size < static_cast<tsize_t>(left) ? size : left);
    std::memcpy(buf, m->data->data() + m->pos, static_cast<size_t>(n));
    m->pos += static_cast<toff_t>(n);
    return n;
}
tsize_t MemWrite(thandle_t, tdata_t, tsize_t) { return 0; }
toff_t MemSeek(thandle_t h, toff_t off, int whence)
{
    MemStream *m = static_cast<MemStream *>(h);
    if (whence == SEEK_SET)
        m->pos = off;
    else if (whence == SEEK_CUR)
        m->pos += off;
    else
        m->pos = m->data->size() + off;
    return m->pos;
}
int MemClose(thandle_t) { return 0; }
toff_t MemSize(thandle_t h) { return static_cast<MemStream *>(h)->data->size(); }
int MemMap(thandle_t, tdata_t *, toff_t *) { return 0; }
void MemUnmap(thandle_t, tdata_t, toff_t) {}

openrider::sdk::Result<Image> DecodeTiff(const std::string &bytes)
{
    TIFFSetErrorHandler(nullptr);
    TIFFSetWarningHandler(nullptr);
    MemStream m{&bytes, 0};
    TIFF *t = TIFFClientOpen("tile", "r", &m, MemRead, MemWrite, MemSeek, MemClose,
                             MemSize, MemMap, MemUnmap);
    if (!t)
        return openrider::sdk::Result<Image>::Failure("not a readable TIFF");
    uint32_t w = 0, h = 0;
    TIFFGetField(t, TIFFTAG_IMAGEWIDTH, &w);
    TIFFGetField(t, TIFFTAG_IMAGELENGTH, &h);
    Image img;
    img.width = static_cast<int>(w);
    img.height = static_cast<int>(h);
    img.rgba.resize(static_cast<size_t>(w) * h * 4);
    const int ok = TIFFReadRGBAImageOriented(t, w, h, reinterpret_cast<uint32_t *>(img.rgba.data()),
                                             ORIENTATION_TOPLEFT, 0);
    TIFFClose(t);
    if (!ok)
        return openrider::sdk::Result<Image>::Failure("cannot decode TIFF");
    return openrider::sdk::Result<Image>::Success(std::move(img));
}

} // namespace

// @impl 0f87-6acd-2f33-87c2
openrider::sdk::Result<Image> DecodeTile(const std::string &bytes)
{
    if (bytes.empty())
        return openrider::sdk::Result<Image>::Failure("empty tile");
    if (IsTiff(bytes))
        return DecodeTiff(bytes);
    int w = 0, h = 0, n = 0;
    unsigned char *px = stbi_load_from_memory(reinterpret_cast<const unsigned char *>(bytes.data()),
                                              static_cast<int>(bytes.size()), &w, &h, &n, 4);
    if (!px)
        return openrider::sdk::Result<Image>::Failure(std::string("cannot decode image: ") +
                                            stbi_failure_reason());
    Image img;
    img.width = w;
    img.height = h;
    img.rgba.assign(px, px + static_cast<size_t>(w) * h * 4);
    stbi_image_free(px);
    return openrider::sdk::Result<Image>::Success(std::move(img));
}

} // namespace maptiles
