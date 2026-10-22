// WMTS tiles plugin: a layer names a service's capabilities document and
// one or more of its WMTS layers (a provider often publishes the same map
// as one layer per coordinate system); the capabilities give the tile
// matrix sets -- Web Mercator and the provider's national grids -- which
// become the layer's grids.

#include "wmts_layer.h"

#include <openrider/plugin.h>
#include <openrider/tiles.h>

#include <cJSON.h>

#include <cstdio>
#include <memory>
#include <string>

namespace {

void SetError(char *err, size_t errlen, const std::string &message)
{
    if (err && errlen)
        std::snprintf(err, errlen, "%s", message.c_str());
}

std::string Text(const cJSON *object, const char *key, const char *fallback = "")
{
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(object, key);
    return cJSON_IsString(value) ? value->valuestring : fallback;
}

double Number(const cJSON *object, const char *key, double fallback)
{
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(object, key);
    return cJSON_IsNumber(value) ? value->valuedouble : fallback;
}

void *WmtsOpen(const char *layer_json, char *err, size_t errlen)
{
    cJSON *root = cJSON_Parse(layer_json ? layer_json : "");
    if (!cJSON_IsObject(root) || Text(root, "protocol") != "wmts") {
        cJSON_Delete(root);
        SetError(err, errlen, "not a wmts layer");
        return nullptr;
    }
    WmtsLayer *layer = new WmtsLayer;
    layer->id = Text(root, "id");
    layer->name = Text(root, "name", layer->id.c_str());
    layer->attribution = Text(root, "attribution");
    layer->capabilities_url = Text(root, "capabilities");
    const cJSON *item = nullptr;
    cJSON_ArrayForEach(item, cJSON_GetObjectItemCaseSensitive(root, "layers")) {
        if (cJSON_IsString(item))
            layer->layers.push_back(item->valuestring);
    }
    cJSON_ArrayForEach(item, cJSON_GetObjectItemCaseSensitive(root, "headers")) {
        if (cJSON_IsString(item))
            layer->headers += std::string(item->string) + ": " + item->valuestring + "\n";
    }
    layer->info.min_zoom = static_cast<int>(Number(root, "min_zoom", 0));
    layer->info.max_zoom = static_cast<int>(Number(root, "max_zoom", 18));
    layer->info.tile_size = static_cast<int>(Number(root, "tile_size", 256));
    const cJSON *extent = cJSON_GetObjectItemCaseSensitive(root, "extent");
    const double world[4] = {-180.0, -85.0511287798, 180.0, 85.0511287798};
    for (int k = 0; k < 4; ++k) {
        const cJSON *value = cJSON_GetArrayItem(extent, k);
        layer->info.extent[k] = cJSON_IsNumber(value) ? value->valuedouble : world[k];
    }
    cJSON_Delete(root);
    if (layer->id.empty() || layer->capabilities_url.empty() || layer->layers.empty()) {
        SetError(err, errlen, "a wmts layer needs an id, a capabilities URL and layers");
        delete layer;
        return nullptr;
    }
    layer->info.id = layer->id.c_str();
    layer->info.name = layer->name.c_str();
    layer->info.crs = "EPSG:3857";
    layer->info.attribution = layer->attribution.c_str();
    layer->info.server_count = 1;
    layer->info.headers = layer->headers.c_str();
    return layer;
}

void WmtsClose(void *handle) { delete static_cast<WmtsLayer *>(handle); }

const or_tile_layer *WmtsInfo(void *handle) { return &static_cast<WmtsLayer *>(handle)->info; }

// Tiles are addressed by grid: the XYZ addressing is not served.
int WmtsXyzUrl(void *, size_t, int, int, int, char *, size_t) { return 1; }

const char *WmtsCapabilitiesUrl(void *handle) { return static_cast<WmtsLayer *>(handle)->capabilities_url.c_str(); }

int WmtsSetCapabilities(void *handle, const char *document, size_t len, char *err, size_t errlen)
{
    WmtsLayer *layer = static_cast<WmtsLayer *>(handle);
    std::string error;
    const std::unique_ptr<XmlNode> root = ParseXml(std::string(document ? document : "", len), error);
    if (!root) {
        SetError(err, errlen, "capabilities: " + error);
        return 1;
    }
    error = ReadGrids(*root, layer->layers, layer->grids, layer->kvp_url);
    if (layer->kvp_url.empty()) {
        // No declared endpoint: the capabilities URL without its query.
        layer->kvp_url = layer->capabilities_url.substr(0, layer->capabilities_url.find('?'));
    }
    layer->described.clear();
    for (const WmtsGrid &grid : layer->grids)
        layer->described.push_back({grid.id.c_str(), grid.crs.c_str(), grid.levels.size(), grid.levels.data()});
    if (!error.empty()) {
        SetError(err, errlen, error);
        return 1;
    }
    return 0;
}

size_t WmtsGridCount(void *handle) { return static_cast<WmtsLayer *>(handle)->described.size(); }

const or_tile_grid *WmtsGridAt(void *handle, size_t index)
{
    WmtsLayer *layer = static_cast<WmtsLayer *>(handle);
    return index < layer->described.size() ? &layer->described[index] : nullptr;
}

int WmtsTileUrl(void *handle, size_t, size_t grid, size_t level, int column, int row, char *url, size_t len)
{
    const WmtsLayer *layer = static_cast<WmtsLayer *>(handle);
    if (grid >= layer->grids.size() || level >= layer->grids[grid].levels.size())
        return 1;
    const std::string built = layer->TileUrl(grid, level, column, row);
    if (built.size() + 1 > len)
        return 1;
    std::snprintf(url, len, "%s", built.c_str());
    return 0;
}

const or_tiles_vtable kVtable = {sizeof(or_tiles_vtable), "wmts",        WmtsOpen,      WmtsClose,
                                 WmtsInfo,                WmtsXyzUrl,    WmtsCapabilitiesUrl, WmtsSetCapabilities,
                                 WmtsGridCount,           WmtsGridAt,     WmtsTileUrl};
const or_plugin_info kInfo = {OR_PLUGIN_API_VERSION, OR_PLUGIN_API_MINOR, OR_PLUGIN_TILES, OR_PLUGIN_THREAD_SAFE,
                              "wmts", "0.1", &kVtable};

} // namespace

// @impl 0f87-6ada-53d7-4404
extern "C" OR_PLUGIN_EXPORT const or_plugin_info *openrider_plugin(void)
{
    return &kInfo;
}
