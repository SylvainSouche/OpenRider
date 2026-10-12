#include "maptiles/fetcher.h"

#include <curl/curl.h>

#include <algorithm>
#include <chrono>

namespace maptiles {

namespace {

double now_s()
{
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

size_t append_body(char *data, size_t size, size_t n, void *user)
{
    static_cast<std::string *>(user)->append(data, size * n);
    return size * n;
}

std::once_flag g_curl_init;

} // namespace

TileFetcher::TileFetcher(const or_tiles_vtable *vt, void *layer, TileCache *cache,
                         std::string user_agent, int workers)
    : vt_(vt), layer_(layer), info_(vt->layer(layer)), cache_(cache),
      user_agent_(std::move(user_agent)), service_(info_->server_count)
{
    std::call_once(g_curl_init, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
    for (int i = 0; i < workers; ++i)
        threads_.emplace_back(&TileFetcher::worker, this);
}

TileFetcher::~TileFetcher()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    cv_.notify_all();
    for (std::thread &t : threads_)
        t.join();
}

void TileFetcher::request(const TileKey &k)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!requested_.insert(k).second)
            return;
        queue_.push_back(k);
        // Keep the queue bounded: the oldest requests are for views long gone.
        while (queue_.size() > kMaxPending) {
            requested_.erase(queue_.front());
            queue_.pop_front();
        }
    }
    cv_.notify_one();
}

std::vector<ReadyTile> TileFetcher::take_ready()
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ReadyTile> out;
    out.swap(ready_);
    return out;
}

size_t TileFetcher::pending() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return requested_.size();
}

bool TileFetcher::unavailable() const
{
    std::lock_guard<std::mutex> lock(service_mutex_);
    return service_.unavailable();
}

// @impl 0f87-6acd-2f33-f327
bool TileFetcher::fetch_network(void *curl, const TileKey &k, std::string &bytes, bool &missing)
{
    // Reserve a start time: one shared delay between loads for the whole
    // service, whatever the number of connections.
    int server;
    double wait;
    {
        std::lock_guard<std::mutex> lock(service_mutex_);
        server = service_.pick_server(now_s());
        if (server < 0)
            server = 0;  // every mirror set aside: the delay still throttles retries
        const double start = std::max(now_s(), service_.next_allowed_s);
        service_.next_allowed_s = start + service_.delay.delay_ms / 1000.0;
        wait = start - now_s();
    }
    if (wait > 0.0) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (cv_.wait_for(lock, std::chrono::duration<double>(wait), [this] { return stop_; }))
            return false;
    }

    char url[2048];
    if (vt_->tile_url(layer_, static_cast<size_t>(server), k.z, k.x, k.y, url, sizeof url) != 0)
        return false;
    CURL *c = static_cast<CURL *>(curl);
    curl_slist *headers = nullptr;
    const std::string extra = info_->headers ? info_->headers : "";
    for (size_t p = 0, e; p < extra.size(); p = e + 1) {
        e = extra.find('\n', p);
        if (e == std::string::npos)
            e = extra.size();
        if (e > p)
            headers = curl_slist_append(headers, extra.substr(p, e - p).c_str());
    }
    bytes.clear();
    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_USERAGENT, user_agent_.c_str());
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, append_body);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &bytes);
    const CURLcode rc = curl_easy_perform(c);
    long status = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(headers);

    const bool ok = rc == CURLE_OK && status == 200 && !bytes.empty();
    missing = rc == CURLE_OK && (status == 404 || status == 204);  // no tile there: not a fault
    {
        std::lock_guard<std::mutex> lock(service_mutex_);
        service_.report(static_cast<size_t>(server), ok || missing, now_s());
    }
    if (ok)
        cache_->put(info_->id, k.z, k.x, k.y, bytes);
    return ok;
}

void TileFetcher::worker()
{
    CURL *curl = curl_easy_init();
    for (;;) {
        TileKey k;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return stop_ || !queue_.empty(); });
            if (stop_)
                break;
            k = queue_.back();  // most recent first: the current view
            queue_.pop_back();
        }
        std::string bytes;
        bool missing = false;
        bool have = cache_->get(info_->id, k.z, k.x, k.y, bytes);
        if (!have && curl)
            have = fetch_network(curl, k, bytes, missing);
        ReadyTile r;
        r.key = k;
        if (have) {
            core::Result<Image> img = decode_tile(bytes);
            if (img.ok())
                r.image = std::move(img.value);
            else
                ++errors_;
        } else if (!missing) {
            ++errors_;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        if (stop_)
            break;
        ready_.push_back(std::move(r));
        requested_.erase(k);
    }
    if (curl)
        curl_easy_cleanup(curl);
}

} // namespace maptiles
