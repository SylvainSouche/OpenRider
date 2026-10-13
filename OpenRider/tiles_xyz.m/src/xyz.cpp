// XYZ tiles plugin: layers whose tiles are addressed by a URL template with
// {z}, {x}, {y} and optionally {s}, which takes each of the layer's server
// names in turn (e.g. "a", "b", "c" for OpenTopoMap).

#include <openrider/plugin.h>
#include <openrider/tiles.h>

#include <cJSON.h>

#include <cstdio>
#include <string>
#include <vector>

namespace {

struct Layer {
    std::string id, name, crs, attribution, url, headers;
    std::vector<std::string> servers;
    or_tile_layer info{};
};

void SetErr(char *err, size_t errlen, const std::string &msg)
{
    if (err && errlen)
        std::snprintf(err, errlen, "%s", msg.c_str());
}

std::string Str(const cJSON *o, const char *k, const char *def = "")
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, k);
    return cJSON_IsString(v) ? v->valuestring : def;
}

double Num(const cJSON *o, const char *k, double def)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, k);
    return cJSON_IsNumber(v) ? v->valuedouble : def;
}

void ReplaceAll(std::string &s, const std::string &from, const std::string &to)
{
    for (size_t p = s.find(from); p != std::string::npos; p = s.find(from, p + to.size()))
        s.replace(p, from.size(), to);
}

void *XyzOpen(const char *layer_json, char *err, size_t errlen)
{
    cJSON *root = cJSON_Parse(layer_json ? layer_json : "");
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        SetErr(err, errlen, "layer definition is not a JSON object");
        return nullptr;
    }
    const std::string protocol = Str(root, "protocol", "xyz");
    if (protocol != "xyz") {
        cJSON_Delete(root);
        SetErr(err, errlen, "not an xyz layer (protocol \"" + protocol + "\")");
        return nullptr;
    }
    Layer *l = new Layer;
    l->id = Str(root, "id");
    l->name = Str(root, "name", l->id.c_str());
    l->crs = Str(root, "crs", "EPSG:3857");
    l->attribution = Str(root, "attribution");
    l->url = Str(root, "url");
    const cJSON *servers = cJSON_GetObjectItemCaseSensitive(root, "servers");
    const cJSON *s = nullptr;
    cJSON_ArrayForEach(s, servers) {
        if (cJSON_IsString(s))
            l->servers.push_back(s->valuestring);
    }
    if (l->servers.empty())
        l->servers.push_back("");
    const cJSON *headers = cJSON_GetObjectItemCaseSensitive(root, "headers");
    const cJSON *h = nullptr;
    cJSON_ArrayForEach(h, headers) {
        if (cJSON_IsString(h))
            l->headers += std::string(h->string) + ": " + h->valuestring + "\n";
    }
    l->info.min_zoom = static_cast<int>(Num(root, "min_zoom", 0));
    l->info.max_zoom = static_cast<int>(Num(root, "max_zoom", 18));
    l->info.tile_size = static_cast<int>(Num(root, "tile_size", 256));
    const cJSON *ext = cJSON_GetObjectItemCaseSensitive(root, "extent");
    const double world[4] = {-180.0, -85.0511287798, 180.0, 85.0511287798};
    for (int i = 0; i < 4; ++i) {
        const cJSON *v = cJSON_GetArrayItem(ext, i);
        l->info.extent[i] = cJSON_IsNumber(v) ? v->valuedouble : world[i];
    }
    cJSON_Delete(root);

    if (l->id.empty() || l->url.find("{z}") == std::string::npos ||
        l->url.find("{x}") == std::string::npos || l->url.find("{y}") == std::string::npos) {
        SetErr(err, errlen, "layer needs an id and a url with {z}, {x} and {y}");
        delete l;
        return nullptr;
    }
    if (l->crs != "EPSG:3857") {
        SetErr(err, errlen, "xyz layers must be EPSG:3857 (Web Mercator), not " + l->crs);
        delete l;
        return nullptr;
    }
    l->info.id = l->id.c_str();
    l->info.name = l->name.c_str();
    l->info.crs = l->crs.c_str();
    l->info.attribution = l->attribution.c_str();
    l->info.server_count = l->servers.size();
    l->info.headers = l->headers.c_str();
    return l;
}

void XyzClose(void *h) { delete static_cast<Layer *>(h); }

const or_tile_layer *XyzLayer(void *h) { return &static_cast<Layer *>(h)->info; }

int XyzTileUrl(void *h, size_t server, int z, int x, int y, char *url, size_t len)
{
    const Layer *l = static_cast<Layer *>(h);
    if (server >= l->servers.size())
        return 1;
    std::string u = l->url;
    ReplaceAll(u, "{s}", l->servers[server]);
    ReplaceAll(u, "{z}", std::to_string(z));
    ReplaceAll(u, "{x}", std::to_string(x));
    ReplaceAll(u, "{y}", std::to_string(y));
    if (u.size() + 1 > len)
        return 1;
    std::snprintf(url, len, "%s", u.c_str());
    return 0;
}

const or_tiles_vtable kVtable = {"xyz", XyzOpen, XyzClose, XyzLayer, XyzTileUrl};
const or_plugin_info kInfo = {OR_PLUGIN_API_VERSION, OR_PLUGIN_TILES, "xyz", "0.1", &kVtable};

} // namespace

// @impl 0f87-6acd-2f33-42fb
extern "C" __attribute__((visibility("default"))) const or_plugin_info *openrider_plugin(void)
{
    return &kInfo;
}
