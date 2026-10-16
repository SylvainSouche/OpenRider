#include "texture_mirror.h"

// glext.h too: Windows' gl.h stops at OpenGL 1.1 (no GL_CLAMP_TO_EDGE).
#define GLFW_INCLUDE_GLEXT
#include <GLFW/glfw3.h>

namespace ui {

TextureMirror::~TextureMirror()
{
    for (auto &texture : textures_)
        glDeleteTextures(1, &texture.second.gl_texture);
}

unsigned int TextureMirror::Texture(const maptiles::TileKey &key, const app::MapTile &tile)
{
    Entry &entry = textures_[key];
    if (entry.gl_texture && entry.generation == tile.generation)
        return entry.gl_texture;
    if (!entry.gl_texture)
        glGenTextures(1, &entry.gl_texture);
    glBindTexture(GL_TEXTURE_2D, entry.gl_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tile.image.width, tile.image.height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 tile.image.rgba.data());
    entry.generation = tile.generation;
    return entry.gl_texture;
}

void TextureMirror::Prune(const app::MapViewModel &view_model)
{
    for (auto it = textures_.begin(); it != textures_.end();) {
        if (view_model.tiles.count(it->first)) {
            ++it;
            continue;
        }
        glDeleteTextures(1, &it->second.gl_texture);
        it = textures_.erase(it);
    }
}

} // namespace ui
