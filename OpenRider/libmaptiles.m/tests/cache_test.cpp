#include <atf-c++.hpp>


#include <atomic>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <vector>
#include "maptiles/cache_eviction.h"
#include "maptiles/tile_cache.h"

namespace {

std::string TempDir()
{
    char tmpl[] = "/tmp/openrider-cache-XXXXXX";
    return mkdtemp(tmpl);
}

bool Exists(const std::string &p)
{
    struct stat st;
    return stat(p.c_str(), &st) == 0;
}

} // namespace

ATF_TEST_CASE_WITHOUT_HEAD(lru_victims_oldest_first);
ATF_TEST_CASE_BODY(lru_victims_oldest_first)
{
    const std::vector<maptiles::CacheEntry> e = {
        {"new", 400, 300}, {"old", 400, 100}, {"mid", 400, 200}};
    ATF_REQUIRE(maptiles::LruVictims(e, 1200).empty());
    const auto v = maptiles::LruVictims(e, 500);
    ATF_REQUIRE_EQ(v.size(), 2u);
    ATF_REQUIRE_EQ(v[0], "old");
    ATF_REQUIRE_EQ(v[1], "mid");
}

ATF_TEST_CASE_WITHOUT_HEAD(put_then_get);
ATF_TEST_CASE_BODY(put_then_get)
{
    maptiles::TileCache c(TempDir(), 1 << 20);
    ATF_REQUIRE(c.Put("opentopomap", 12, 2123, 1459, "tile-bytes").Ok());
    std::string got;
    ATF_REQUIRE(c.Get("opentopomap", 12, 2123, 1459, got));
    ATF_REQUIRE_EQ(got, "tile-bytes");
    ATF_REQUIRE(!c.Get("opentopomap", 12, 0, 0, got));
    ATF_REQUIRE(c.Path("opentopomap", 12, 2123, 1459).find("/opentopomap/12/2123/1459.tile") !=
                std::string::npos);
}

ATF_TEST_CASE_WITHOUT_HEAD(eviction_keeps_under_maximum);
ATF_TEST_CASE_BODY(eviction_keeps_under_maximum)
{
    maptiles::TileCache c(TempDir(), 25);
    const std::string ten(10, 'x');
    for (int y = 0; y < 5; ++y)
        ATF_REQUIRE(c.Put("l", 1, 0, y, ten).Ok());
    int kept = 0;
    for (int y = 0; y < 5; ++y)
        kept += Exists(c.Path("l", 1, 0, y)) ? 1 : 0;
    ATF_REQUIRE(kept <= 2);  // 25 bytes hold at most two 10-byte tiles
    ATF_REQUIRE(Exists(c.Path("l", 1, 0, 4)));  // the latest one survives
}

// Writers on several threads, as the fetch workers: the cache ends within
// its maximum, every write lands, and no temporary file is left.
ATF_TEST_CASE_WITHOUT_HEAD(concurrent_writers_stay_under_maximum);
ATF_TEST_CASE_BODY(concurrent_writers_stay_under_maximum)
{
    const std::string root = TempDir();
    maptiles::TileCache c(root, 1000);
    const std::string hundred(100, 'x');
    std::vector<std::thread> writers;
    std::atomic<int> failures{0};
    for (int w = 0; w < 4; ++w)
        writers.emplace_back([&, w] {
            for (int y = 0; y < 50; ++y)
                if (!c.Put("l", 1, w, y, hundred).Ok())
                    ++failures;
        });
    for (std::thread &writer : writers)
        writer.join();
    ATF_REQUIRE_EQ(failures.load(), 0);
    c.Evict();
    uint64_t total = 0;
    for (int w = 0; w < 4; ++w)
        for (int y = 0; y < 50; ++y) {
            struct stat st;
            if (stat(c.Path("l", 1, w, y).c_str(), &st) == 0)
                total += static_cast<uint64_t>(st.st_size);
        }
    ATF_REQUIRE(total <= 1000);
    const std::string listing = "find " + root + " -name '*.tmp*' | grep -q .";
    ATF_REQUIRE(std::system(listing.c_str()) != 0);  // no temporary file left
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, lru_victims_oldest_first);
    ATF_ADD_TEST_CASE(tcs, put_then_get);
    ATF_ADD_TEST_CASE(tcs, eviction_keeps_under_maximum);
    ATF_ADD_TEST_CASE(tcs, concurrent_writers_stay_under_maximum);
}
