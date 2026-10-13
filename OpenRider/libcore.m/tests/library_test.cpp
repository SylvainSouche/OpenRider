#include <atf-c++.hpp>

#include "core/library.h"
#include "core/outing_selection.h"

#include <chrono>
#include <cstdio>
#include <thread>

namespace {

core::Outing Make(const char *date, const char *name, const char *sport, double dist)
{
    core::Outing o{};
    std::snprintf(o.raw.start_local, sizeof o.raw.start_local, "%s 10:00:00", date);
    std::snprintf(o.raw.name, sizeof o.raw.name, "%s", name);
    o.raw.distance_m = dist;
    o.type = core::ActivityType{sport, "generic", ""};
    return o;
}

std::vector<core::Outing> Sample()
{
    return {Make("2026-01-05", "B", "hiking", 12000), Make("2025-12-24", "A", "running", 8000),
            Make("2026-03-01", "C", "hiking", 3000)};
}

// A fake input plugin delivering three outings.
int FakeList(void *, or_outing_cb cb, void *user, char *, size_t)
{
    for (size_t i = 0; i < 3; ++i) {
        or_outing o{};
        std::snprintf(o.id, sizeof o.id, "%zu", i);
        std::snprintf(o.fit_sport, sizeof o.fit_sport, "running");
        std::snprintf(o.source_type, sizeof o.source_type, i == 1 ? "backcountry_skiing" : "");
        if (cb(user, &o, i, 3) != 0)
            break;
    }
    return 0;
}

int FakeFail(void *, or_outing_cb, void *, char *err, size_t errlen)
{
    std::snprintf(err, errlen, "database is locked");
    return 1;
}

void WaitDone(const core::Library &lib)
{
    for (int i = 0; i < 500 && lib.Scanning(); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
}

} // namespace

ATF_TEST_CASE_WITHOUT_HEAD(filter_by_type_and_dates);
ATF_TEST_CASE_BODY(filter_by_type_and_dates)
{
    const auto v = Sample();
    core::OutingFilter f;
    f.type_key = "hiking/generic";
    ATF_REQUIRE_EQ(core::SelectOutings(v, f, core::SortKey::Date, true).size(), 2u);
    f.from_date = "2026-02-01";
    const auto r = core::SelectOutings(v, f, core::SortKey::Date, true);
    ATF_REQUIRE_EQ(r.size(), 1u);
    ATF_REQUIRE_EQ(r[0], 2u);
    core::OutingFilter g;
    g.to_date = "2025-12-31";  // inclusive day bound
    ATF_REQUIRE_EQ(core::SelectOutings(v, g, core::SortKey::Date, true).size(), 1u);
}

ATF_TEST_CASE_WITHOUT_HEAD(sort_orders);
ATF_TEST_CASE_BODY(sort_orders)
{
    const auto v = Sample();
    const core::OutingFilter all;
    auto r = core::SelectOutings(v, all, core::SortKey::Date, false);
    ATF_REQUIRE(r == (std::vector<size_t>{2, 0, 1}));
    r = core::SelectOutings(v, all, core::SortKey::Distance, true);
    ATF_REQUIRE(r == (std::vector<size_t>{2, 1, 0}));
    r = core::SelectOutings(v, all, core::SortKey::Name, true);
    ATF_REQUIRE(r == (std::vector<size_t>{1, 0, 2}));
}

ATF_TEST_CASE_WITHOUT_HEAD(background_scan_resolves_types);
ATF_TEST_CASE_BODY(background_scan_resolves_types)
{
    core::TypeCatalog cat;
    cat.LoadSource("garmin_connect",
                    "{\"backcountry_skiing\":{\"sport\":\"alpine_skiing\",\"sub_sport\":\"backcountry\"}}");
    or_input_vtable vt{nullptr, nullptr, FakeList, nullptr};
    core::Library lib;
    lib.StartScan(&vt, nullptr, &cat, "garmin_connect");
    WaitDone(lib);
    std::vector<core::Outing> got;
    ATF_REQUIRE_EQ(lib.Take(got), "");
    ATF_REQUIRE_EQ(got.size(), 3u);
    ATF_REQUIRE_EQ(got[0].type.Key(), "running/generic");
    ATF_REQUIRE_EQ(got[1].type.Key(), "alpine_skiing/backcountry");
    ATF_REQUIRE(lib.Progress() == 1.0f);
}

ATF_TEST_CASE_WITHOUT_HEAD(background_scan_reports_error);
ATF_TEST_CASE_BODY(background_scan_reports_error)
{
    core::TypeCatalog cat;
    or_input_vtable vt{nullptr, nullptr, FakeFail, nullptr};
    core::Library lib;
    lib.StartScan(&vt, nullptr, &cat, "garmin_connect");
    WaitDone(lib);
    std::vector<core::Outing> got;
    ATF_REQUIRE_EQ(lib.Take(got), "database is locked");
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, filter_by_type_and_dates);
    ATF_ADD_TEST_CASE(tcs, sort_orders);
    ATF_ADD_TEST_CASE(tcs, background_scan_resolves_types);
    ATF_ADD_TEST_CASE(tcs, background_scan_reports_error);
}
