/// @file
/// OpenGL textures of the map tiles.
#pragma once

#include "app/map_view_controller.h"

#include <map>

namespace ui {

/// OpenGL textures mirroring the map view model's decoded tiles. A
/// rendering cache private to the views (like ImGui's own font atlas): it
/// holds no state anyone observes, and is rebuilt from the view model.
class TextureMirror {
public:
    /// Deletes every texture; needs the GL context.
    ~TextureMirror();

    /// Texture of a decoded tile, uploaded on first use or when the tile's
    /// image changed (new generation).
    /// @return the OpenGL texture name
    unsigned int Texture(const maptiles::TileKey &key, const app::MapTile &tile);

    /// Drops the textures of tiles gone from the view model.
    void Prune(const app::MapViewModel &view_model);

private:
    /// One uploaded texture.
    struct Entry {
        unsigned int gl_texture = 0;   ///< OpenGL texture name, 0 = none
        unsigned long generation = 0;  ///< MapTile::generation uploaded
    };
    std::map<maptiles::TileKey, Entry> textures_;  ///< by tile
};

} // namespace ui
