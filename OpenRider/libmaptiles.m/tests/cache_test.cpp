#include <atf-c++.hpp>

#include "maptiles/cache.h"

#include <cstdlib>
#include <string>
#include <sys/stat.h>

namespace {

std::string temp_dir()
{
    char tmpl[] = "/tmp/openrider-cache-XXXXXX";
    return mkdtemp(tmpl);
}

bool exists(const std::string &p)
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
    ATF_REQUIRE(maptiles::lru_victims(e, 1200).empty());
    const auto v = maptiles::lru_victims(e, 500);
    ATF_REQUIRE_EQ(v.size(), 2u);
    ATF_REQUIRE_EQ(v[0], "old");
    ATF_REQUIRE_EQ(v[1], "mid");
}

ATF_TEST_CASE_WITHOUT_HEAD(put_then_get);
ATF_TEST_CASE_BODY(put_then_get)
{
    maptiles::TileCache c(temp_dir(), 1 << 20);
    ATF_REQUIRE(c.put("opentopomap", 12, 2123, 1459, "tile-bytes").ok());
    std::string got;
    ATF_REQUIRE(c.get("opentopomap", 12, 2123, 1459, got));
    ATF_REQUIRE_EQ(got, "tile-bytes");
    ATF_REQUIRE(!c.get("opentopomap", 12, 0, 0, got));
    ATF_REQUIRE(c.path("opentopomap", 12, 2123, 1459).find("/opentopomap/12/2123/1459.tile") !=
                std::string::npos);
}

ATF_TEST_CASE_WITHOUT_HEAD(eviction_keeps_under_maximum);
ATF_TEST_CASE_BODY(eviction_keeps_under_maximum)
{
    maptiles::TileCache c(temp_dir(), 25);
    const std::string ten(10, 'x');
    for (int y = 0; y < 5; ++y)
        ATF_REQUIRE(c.put("l", 1, 0, y, ten).ok());
    int kept = 0;
    for (int y = 0; y < 5; ++y)
        kept += exists(c.path("l", 1, 0, y)) ? 1 : 0;
    ATF_REQUIRE(kept <= 2);  // 25 bytes hold at most two 10-byte tiles
    ATF_REQUIRE(exists(c.path("l", 1, 0, 4)));  // the latest one survives
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, lru_victims_oldest_first);
    ATF_ADD_TEST_CASE(tcs, put_then_get);
    ATF_ADD_TEST_CASE(tcs, eviction_keeps_under_maximum);
}
