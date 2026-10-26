#include "maptiles/tile_fetcher.h"

#include "core/plugin_thread.h"

#include <curl/curl.h>

#include <algorithm>
#include <chrono>
#include <cstring>

namespace maptiles {

namespace {

// Monotonic time (s).
double NowS()
{
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

// libcurl write callback: appends the received data to the std::string `user`.
size_t AppendBody(char *data, size_t size, size_t count, void *user)
{
    static_cast<std::string *>(user)->append(data, size * count);
    return size * count;
}

std::once_flag g_curl_init;  // libcurl is initialized once per process

/// The description of `layer`, asked of its plugin.
const or_tile_layer *LayerOf(const or_tiles_vtable *tiles_vtable, void *layer)
{
    const or_tile_layer *info = nullptr;
    core::CallPlugin(tiles_vtable, [&] { info = tiles_vtable->layer(layer); });
    return info;
}

} // namespace

TileFetcher::TileFetcher(const or_tiles_vtable *tiles_vtable, void *layer, std::vector<TileGrid> grids,
                         TileCache *cache, std::string user_agent, int workers)
    : tiles_vtable_(tiles_vtable), layer_(layer), layer_info_(LayerOf(tiles_vtable, layer)), grids_(std::move(grids)),
      cache_(cache), user_agent_(std::move(user_agent)), service_(layer_info_->server_count)
{
    std::call_once(g_curl_init, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
    for (int i = 0; i < workers; ++i)
        threads_.emplace_back(&TileFetcher::Worker, this);
}

TileFetcher::~TileFetcher()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    wake_.notify_all();
    for (std::thread &thread : threads_)
        thread.join();
}

void TileFetcher::Request(const TileKey &key)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!requested_.insert(key).second)
            return;
        queue_.push_back(key);
        // Keep the queue bounded: the oldest requests are for views long gone.
        while (queue_.size() > kMaxPending) {
            requested_.erase(queue_.front());
            queue_.pop_front();
        }
    }
    wake_.notify_one();
}

std::vector<ReadyTile> TileFetcher::TakeReady()
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ReadyTile> taken;
    taken.swap(ready_);
    return taken;
}

size_t TileFetcher::Pending() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return requested_.size();
}

bool TileFetcher::Unavailable() const
{
    std::lock_guard<std::mutex> lock(service_mutex_);
    return service_.Unavailable();
}

std::string TileFetcher::CacheNameOf(const TileKey &key) const
{
    return key.grid >= 0 && key.grid < static_cast<int>(grids_.size())
               ? CacheName(layer_info_->id, grids_[static_cast<size_t>(key.grid)])
               : std::string(layer_info_->id);
}

// @impl 0f87-6acd-2f33-f327
bool TileFetcher::FetchNetwork(void *curl, const TileKey &key, std::string &bytes, bool &missing)
{
    // Reserve a start time: one shared delay between loads for the whole
    // service, whatever the number of connections. The reservation is made
    // under the service lock; the wait happens outside it, on the queue's
    // condition variable so that stopping interrupts it.
    int server;
    double wait;
    {
        std::lock_guard<std::mutex> lock(service_mutex_);
        server = service_.PickServer(NowS());
        if (server < 0)
            server = 0;  // every mirror set aside: the delay still throttles retries
        const double start = std::max(NowS(), service_.next_allowed_s);
        service_.next_allowed_s = start + service_.delay.delay_ms / 1000.0;
        wait = start - NowS();
    }
    if (wait > 0.0) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (wake_.wait_for(lock, std::chrono::duration<double>(wait), [this] { return stop_; }))
            return false;
    }

    char url[2048];
    int rc = 0;
    if (key.grid < 0 || key.grid >= static_cast<int>(grids_.size()))
        return false;
    const bool xyz = grids_[static_cast<size_t>(key.grid)].xyz;
    core::CallPlugin(tiles_vtable_, [&] {
        rc = xyz ? tiles_vtable_->tile_url(layer_, static_cast<size_t>(server), key.z, key.x, key.y, url, sizeof url)
                 : tiles_vtable_->grid_tile_url(layer_, static_cast<size_t>(server), static_cast<size_t>(key.grid),
                                                static_cast<size_t>(key.z), key.x, key.y, url, sizeof url);
    });
    if (rc != 0)
        return false;
    CURL *handle = static_cast<CURL *>(curl);
    curl_slist *headers = nullptr;
    // The layer's extra headers: one "Name: value" per line.
    const std::string extra = layer_info_->headers ? layer_info_->headers : "";
    for (size_t start = 0, end; start < extra.size(); start = end + 1) {
        end = extra.find('\n', start);
        if (end == std::string::npos)
            end = extra.size();
        if (end > start)
            headers = curl_slist_append(headers, extra.substr(start, end - start).c_str());
    }
    bytes.clear();
	curl_easy_setopt(handle, CURLOPT_SSL_OPTIONS, (long)CURLSSLOPT_NATIVE_CA);
    curl_easy_setopt(handle, CURLOPT_URL, url);
    curl_easy_setopt(handle, CURLOPT_USERAGENT, user_agent_.c_str());
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(handle, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(handle, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, AppendBody);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &bytes);
    const CURLcode code = curl_easy_perform(handle);
    long status = 0;
    curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(headers);

    // A local tile set (file://) has no HTTP status; a missing file is a
    // missing tile.
    const bool local = std::strncmp(url, "file://", 7) == 0;
    const bool downloaded = code == CURLE_OK && (status == 200 || (local && status == 0)) && !bytes.empty();
    missing = (code == CURLE_OK && (status == 404 || status == 204)) || (local && code == CURLE_FILE_COULDNT_READ_FILE);  // no tile there: not a fault
    {
        std::lock_guard<std::mutex> lock(service_mutex_);
        service_.Report(static_cast<size_t>(server), downloaded || missing, NowS());
    }
    if (downloaded)
        cache_->Put(CacheNameOf(key), key.z, key.x, key.y, bytes);
    return downloaded;
}

void TileFetcher::Worker()
{
    CURL *curl = curl_easy_init();
    for (;;) {
        TileKey key;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            wake_.wait(lock, [this] { return stop_ || !queue_.empty(); });
            if (stop_)
                break;
            key = queue_.back();  // most recent first: the current view
            queue_.pop_back();
        }
        std::string bytes;
        bool missing = false;
        bool have_bytes = cache_->Get(CacheNameOf(key), key.z, key.x, key.y, bytes);
        if (!have_bytes && curl)
            have_bytes = FetchNetwork(curl, key, bytes, missing);
        ReadyTile ready;
        ready.key = key;
        if (have_bytes) {
            openrider::sdk::Result<Image> image = DecodeTile(bytes);
            if (image.Ok())
                ready.image = std::move(image.value);
            else
                ++errors_;
        } else if (!missing) {
            ++errors_;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        if (stop_)
            break;
        ready_.push_back(std::move(ready));
        requested_.erase(key);
    }
    if (curl)
        curl_easy_cleanup(curl);
}

} // namespace maptiles
