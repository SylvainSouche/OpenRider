#include <atf-c++.hpp>

#include "core/library.h"
#include "core/outing_selection.h"

#include <chrono>
#include <cmath>
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

// Two outings dated, with distances: the GarminDB side of a duplicate.
int FakeDated(void *, or_outing_cb cb, void *user, char *, size_t)
{
    const char *starts[] = {"2026-05-01 10:00:00", "2026-05-02 10:00:00"};
    for (size_t i = 0; i < 2; ++i) {
        or_outing o{};
        std::snprintf(o.id, sizeof o.id, "g%zu", i);
        std::snprintf(o.start_local, sizeof o.start_local, "%s", starts[i]);
        std::snprintf(o.fit_sport, sizeof o.fit_sport, "running");
        o.distance_m = 10000.0;
        cb(user, &o, i, 2);
    }
    return 0;
}

// A GPX folder: the first GarminDB outing again (30 s later, 2% longer), and a
// new one typed "hike".
int FakeGpx(void *, or_outing_cb cb, void *user, char *, size_t)
{
    or_outing same{}, other{};
    std::snprintf(same.id, sizeof same.id, "gpx:1");
    std::snprintf(same.start_local, sizeof same.start_local, "2026-05-01 10:00:30");
    same.distance_m = 10200.0;
    std::snprintf(other.id, sizeof other.id, "gpx:2");
    std::snprintf(other.start_local, sizeof other.start_local, "2026-05-03 09:00:00");
    std::snprintf(other.source_type, sizeof other.source_type, "hike");
    other.distance_m = 5000.0;
    cb(user, &same, 0, 2);
    cb(user, &other, 1, 2);
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

ATF_TEST_CASE_WITHOUT_HEAD(missing_values_sort_last);
ATF_TEST_CASE_BODY(missing_values_sort_last)
{
    // The input ABI gives NaN for a value the source does not have.
    std::vector<core::Outing> v = Sample();
    v.push_back(Make("2026-02-01", "D", "running", std::nan("")));
    v.push_back(Make("2026-02-02", "E", "running", 5000));
    const core::OutingFilter all;
    auto r = core::SelectOutings(v, all, core::SortKey::Distance, true);
    ATF_REQUIRE(r == (std::vector<size_t>{2, 4, 1, 0, 3}));
    r = core::SelectOutings(v, all, core::SortKey::Distance, false);
    ATF_REQUIRE(r == (std::vector<size_t>{0, 1, 4, 2, 3}));  // still last
}

ATF_TEST_CASE_WITHOUT_HEAD(background_scan_resolves_types);
ATF_TEST_CASE_BODY(background_scan_resolves_types)
{
    core::TypeCatalog cat;
    cat.LoadSource("garmin_connect",
                    "{\"backcountry_skiing\":{\"sport\":\"alpine_skiing\",\"sub_sport\":\"backcountry\"}}");
    or_input_vtable vt{sizeof(or_input_vtable), nullptr, nullptr, FakeList, nullptr};
    core::InputSet inputs;
    inputs.Add({&vt, nullptr, "fake", "garmin_connect", false});
    core::Library lib;
    lib.StartScan(&inputs, &cat);
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
    or_input_vtable vt{sizeof(or_input_vtable), nullptr, nullptr, FakeFail, nullptr};
    core::InputSet inputs;
    inputs.Add({&vt, nullptr, "fake", "garmin_connect", false});
    core::Library lib;
    lib.StartScan(&inputs, &cat);
    WaitDone(lib);
    std::vector<core::Outing> got;
    ATF_REQUIRE_EQ(lib.Take(got), "fake: database is locked");
}

// Two sources: the earlier one wins over a duplicate; each outing knows
// its source; types resolve with the source's table, to be confirmed for
// the GPX one.
ATF_TEST_CASE_WITHOUT_HEAD(scan_of_several_sources);
ATF_TEST_CASE_BODY(scan_of_several_sources)
{
    core::TypeCatalog cat;
    cat.LoadSource("gpx", "{\"hike\":{\"sport\":\"hiking\",\"sub_sport\":\"generic\"}}");
    or_input_vtable garmin{sizeof(or_input_vtable), nullptr, nullptr, FakeDated, nullptr};
    or_input_vtable gpx{sizeof(or_input_vtable), nullptr, nullptr, FakeGpx, nullptr};
    core::InputSet inputs;
    inputs.Add({&garmin, nullptr, "garmindb", "garmin_connect", false});
    inputs.Add({&gpx, nullptr, "gpx", "gpx", true});
    core::Library lib;
    lib.StartScan(&inputs, &cat);
    WaitDone(lib);
    std::vector<core::Outing> got;
    ATF_REQUIRE_EQ(lib.Take(got), "");
    ATF_REQUIRE_EQ(got.size(), 3u);  // g0, g1, gpx:2 -- gpx:1 is g0 again
    ATF_REQUIRE_EQ(lib.Hidden(), 1u);
    ATF_REQUIRE_EQ(std::string(got[2].raw.id), "gpx:2");
    ATF_REQUIRE_EQ(got[2].source, 1u);
    ATF_REQUIRE(!got[2].confirmed);
    ATF_REQUIRE(got[0].confirmed);
    ATF_REQUIRE_EQ(got[2].type.Key(), "hiking/generic");
    ATF_REQUIRE_EQ(inputs.SourceOf("gpx:2"), 1u);
    ATF_REQUIRE_EQ(inputs.SourceOf("g1"), 0u);
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, scan_of_several_sources);
    ATF_ADD_TEST_CASE(tcs, missing_values_sort_last);
    ATF_ADD_TEST_CASE(tcs, filter_by_type_and_dates);
    ATF_ADD_TEST_CASE(tcs, sort_orders);
    ATF_ADD_TEST_CASE(tcs, background_scan_resolves_types);
    ATF_ADD_TEST_CASE(tcs, background_scan_reports_error);
}
