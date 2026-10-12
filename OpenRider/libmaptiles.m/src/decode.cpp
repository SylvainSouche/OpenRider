#include "maptiles/decode.h"

#include <stb_image.h>
#include <tiffio.h>

#include <cstring>

namespace maptiles {

namespace {

bool is_tiff(const std::string &b)
{
    return b.size() > 4 && (std::memcmp(b.data(), "II*\0", 4) == 0 ||
                            std::memcmp(b.data(), "MM\0*", 4) == 0);
}

// libtiff reads from memory through these client callbacks.
struct MemStream {
    const std::string *data;
    toff_t pos;
};

tsize_t mem_read(thandle_t h, tdata_t buf, tsize_t size)
{
    MemStream *m = static_cast<MemStream *>(h);
    const toff_t left = m->data->size() > m->pos ? m->data->size() - m->pos : 0;
    const tsize_t n = static_cast<tsize_t>(size < static_cast<tsize_t>(left) ? size : left);
    std::memcpy(buf, m->data->data() + m->pos, static_cast<size_t>(n));
    m->pos += static_cast<toff_t>(n);
    return n;
}
tsize_t mem_write(thandle_t, tdata_t, tsize_t) { return 0; }
toff_t mem_seek(thandle_t h, toff_t off, int whence)
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
int mem_close(thandle_t) { return 0; }
toff_t mem_size(thandle_t h) { return static_cast<MemStream *>(h)->data->size(); }
int mem_map(thandle_t, tdata_t *, toff_t *) { return 0; }
void mem_unmap(thandle_t, tdata_t, toff_t) {}

core::Result<Image> decode_tiff(const std::string &bytes)
{
    TIFFSetErrorHandler(nullptr);
    TIFFSetWarningHandler(nullptr);
    MemStream m{&bytes, 0};
    TIFF *t = TIFFClientOpen("tile", "r", &m, mem_read, mem_write, mem_seek, mem_close,
                             mem_size, mem_map, mem_unmap);
    if (!t)
        return core::Result<Image>::failure("not a readable TIFF");
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
        return core::Result<Image>::failure("cannot decode TIFF");
    return core::Result<Image>::success(std::move(img));
}

} // namespace

// @impl 0f87-6acd-2f33-87c2
core::Result<Image> decode_tile(const std::string &bytes)
{
    if (bytes.empty())
        return core::Result<Image>::failure("empty tile");
    if (is_tiff(bytes))
        return decode_tiff(bytes);
    int w = 0, h = 0, n = 0;
    unsigned char *px = stbi_load_from_memory(reinterpret_cast<const unsigned char *>(bytes.data()),
                                              static_cast<int>(bytes.size()), &w, &h, &n, 4);
    if (!px)
        return core::Result<Image>::failure(std::string("cannot decode image: ") +
                                            stbi_failure_reason());
    Image img;
    img.width = w;
    img.height = h;
    img.rgba.assign(px, px + static_cast<size_t>(w) * h * 4);
    stbi_image_free(px);
    return core::Result<Image>::success(std::move(img));
}

} // namespace maptiles
