#include "maptiles/capabilities.h"

#include "core/plugin_thread.h"
#include "maptiles/tile_grid.h"

#include <curl/curl.h>

#include <cstdio>
#include <ctime>
#include <mutex>
#include <sys/stat.h>

namespace maptiles {

namespace {

// libcurl write callback: appends the received data to the std::string `user`.
size_t AppendBody(char *data, size_t size, size_t count, void *user)
{
    static_cast<std::string *>(user)->append(data, size * count);
    return size * count;
}

// Whole content of a file; false if it cannot be read.
bool ReadFile(const std::string &path, std::string &text)
{
    FILE *file = std::fopen(path.c_str(), "rb");
    if (!file)
        return false;
    char chunk[8192];
    size_t got;
    while ((got = std::fread(chunk, 1, sizeof chunk, file)) > 0)
        text.append(chunk, got);
    std::fclose(file);
    return !text.empty();
}

// Writes `text` to `path` atomically, creating its directory.
void WriteFile(const std::string &directory, const std::string &path, const std::string &text)
{
    mkdir(directory.c_str(), 0755);
    const std::string temporary = path + ".tmp";
    if (FILE *file = std::fopen(temporary.c_str(), "wb")) {
        const bool written = std::fwrite(text.data(), 1, text.size(), file) == text.size();
        if (std::fclose(file) == 0 && written)
            std::rename(temporary.c_str(), path.c_str());
        else
            std::remove(temporary.c_str());
    }
}

// Downloads `url` with the layer's headers ("Name: value" lines).
bool Download(const std::string &url, const std::string &extra_headers, const std::string &user_agent,
              std::string &body)
{
    static std::once_flag curl_init;
    std::call_once(curl_init, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
    CURL *curl = curl_easy_init();
    if (!curl)
        return false;
    curl_slist *headers = nullptr;
    for (size_t start = 0, end; start < extra_headers.size(); start = end + 1) {
        end = extra_headers.find('\n', start);
        if (end == std::string::npos)
            end = extra_headers.size();
        if (end > start)
            headers = curl_slist_append(headers, extra_headers.substr(start, end - start).c_str());
    }
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, AppendBody);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    const CURLcode code = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    // A local file (file://) has no HTTP status.
    const bool local = url.compare(0, 7, "file://") == 0;
    return code == CURLE_OK && (status == 200 || (local && status == 0)) && !body.empty();
}

} // namespace

std::string ProvideCapabilities(const or_tiles_vtable *tiles_vtable, void *layer, const std::string &cache_dir,
                                const std::string &user_agent)
{
    if (!HasGridFunctions(tiles_vtable) || !tiles_vtable->capabilities_url || !tiles_vtable->set_capabilities)
        return "";
    std::string url, layer_id, extra_headers;
    core::CallPlugin(tiles_vtable, [&] {
        const char *asked = tiles_vtable->capabilities_url(layer);
        url = asked ? asked : "";
        const or_tile_layer *info = tiles_vtable->layer(layer);
        layer_id = info->id;
        extra_headers = info->headers ? info->headers : "";
    });
    if (url.empty())
        return "";
    const std::string directory = cache_dir + "/" + layer_id, path = directory + "/capabilities.xml";
    std::string document;
    struct stat info;
    const bool cached = stat(path.c_str(), &info) == 0;
    const bool fresh = cached && std::difftime(std::time(nullptr), info.st_mtime) < kCapabilitiesMaxAge;
    if (!(fresh && ReadFile(path, document))) {
        document.clear();
        if (Download(url, extra_headers, user_agent, document)) {
            mkdir(cache_dir.c_str(), 0755);
            WriteFile(directory, path, document);
        } else if (!(cached && ReadFile(path, document))) {
            return "cannot download " + url;
        }
    }
    char error[256] = "";
    int rc = 0;
    core::CallPlugin(tiles_vtable, [&] {
        rc = tiles_vtable->set_capabilities(layer, document.data(), document.size(), error, sizeof error);
    });
    return rc == 0 ? "" : std::string(error[0] ? error : "capabilities refused");
}

} // namespace maptiles
