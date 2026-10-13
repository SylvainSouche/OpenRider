// AppController with fake input and filter plugins: the model only changes
// through intents and ticks.

#include <atf-c++.hpp>

#include "app/dispatch.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <thread>

namespace {

// Three outings, 100 points each (1 s apart, 2 m/s north).
int FakeList(void *, or_outing_cb cb, void *user, char *, size_t)
{
    for (size_t i = 0; i < 3; ++i) {
        or_outing o{};
        std::snprintf(o.id, sizeof o.id, "id%zu", i);
        std::snprintf(o.name, sizeof o.name, "Outing %zu", i);
        std::snprintf(o.start_local, sizeof o.start_local, "2026-0%zu-10 10:00:00", i + 1);
        std::snprintf(o.fit_sport, sizeof o.fit_sport, i == 2 ? "cycling" : "running");
        cb(user, &o, i, 3);
    }
    return 0;
}

int FakePoints(void *, const char *, or_point_cb cb, void *user, char *, size_t)
{
    for (int i = 0; i < 100; ++i) {
        or_point p{};
        p.t_s = i;
        p.lat = 45.9 + i * 2.0 * 9e-6;
        p.lon = 6.6;
        p.alt_m = 1000.0 + i;
        p.speed_ms = 2.0;
        cb(user, &p);
    }
    return 0;
}

const or_input_vtable kInput = {nullptr, nullptr, FakeList, FakePoints};

// Keeps every other point.
int HalfFilter(const or_point *, size_t n, const double *, unsigned char *keep, char *, size_t)
{
    for (size_t i = 0; i < n; ++i)
        keep[i] = i % 2 == 0 || i + 1 == n;
    return 0;
}
const or_filter_param kParams[] = {{"k", "K", 1.0, 0.0, 2.0}};
const or_filter_vtable kHalf = {OR_FILTER_TRACK, "half", "Half", 1, kParams, HalfFilter};

struct Fixture {
    app::AppModel model;
    app::AppController app{model};
    app::MainViewController main;
    app::ListViewController list;
    app::MapViewController map;
    app::OutingViewController outing;
    app::Controllers c{model, app, main, list, map, outing};
    app::Intents intents;

    Fixture()
    {
        app.SetInput(&kInput, nullptr, "fake");
        app.AddFilter(&kHalf);
        app.Start();
        for (int i = 0; i < 500 && (model.outings.size() < 3 || model.scanning || model.tracks_loading ||
                                    model.tracks.size() < 3);
             ++i) {
            app::Step(c, intents);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
    void Send(app::Intent in)
    {
        intents.Push(std::move(in));
        app::Dispatch(c, intents);
    }
    void Send(app::IntentKind k, int i = 0, int j = 0, double x = 0.0)
    {
        intents.Push(k, i, j, x);
        app::Dispatch(c, intents);
    }
};

} // namespace

ATF_TEST_CASE_WITHOUT_HEAD(scan_and_overview_fill_the_model);
ATF_TEST_CASE_BODY(scan_and_overview_fill_the_model)
{
    Fixture f;
    ATF_REQUIRE_EQ(f.model.outings.size(), 3u);
    ATF_REQUIRE_EQ(f.model.tracks.size(), 3u);
    ATF_REQUIRE_EQ(f.model.status, "3 outings, 3 tracks");
    ATF_REQUIRE_EQ(f.model.selected, -1);
}

ATF_TEST_CASE_WITHOUT_HEAD(selecting_loads_points_and_stats);
ATF_TEST_CASE_BODY(selecting_loads_points_and_stats)
{
    Fixture f;
    f.Send(app::IntentKind::SelectOuting, 1);
    ATF_REQUIRE_EQ(f.model.selected, 1);
    ATF_REQUIRE_EQ(f.model.points_id, "id1");
    ATF_REQUIRE_EQ(f.model.points.size(), 100u);
    ATF_REQUIRE(std::fabs(f.model.computed.distance_m - 198.0) < 2.0);
    f.Send(app::IntentKind::SelectOuting, -1);
    ATF_REQUIRE_EQ(f.model.selected, -1);
}

ATF_TEST_CASE_WITHOUT_HEAD(filters_are_non_destructive);
ATF_TEST_CASE_BODY(filters_are_non_destructive)
{
    Fixture f;
    f.Send(app::IntentKind::SelectOuting, 0);
    f.Send(app::IntentKind::ToggleFilter, 0, 1);
    ATF_REQUIRE_EQ(f.model.points.size(), 51u);
    ATF_REQUIRE_EQ(f.model.raw_points.size(), 100u);  // the original stays
    f.Send(app::IntentKind::ToggleFilter, 0, 0);
    ATF_REQUIRE_EQ(f.model.points.size(), 100u);
    f.Send(app::IntentKind::SetFilterParam, 0, 0, 1.5);
    ATF_REQUIRE_EQ(f.model.filters[0].values[0], 1.5);
    f.Send(app::IntentKind::ResetFilterParams, 0);
    ATF_REQUIRE_EQ(f.model.filters[0].values[0], 1.0);
}

ATF_TEST_CASE_WITHOUT_HEAD(query_drives_list_and_map_filters);
ATF_TEST_CASE_BODY(query_drives_list_and_map_filters)
{
    Fixture f;
    app::Intent type{app::IntentKind::SetTypeFilter};
    type.text = "cycling/generic";
    f.Send(type);
    ATF_REQUIRE_EQ(core::SelectOutings(f.model.outings, f.model.ListFilter(), core::SortKey::Date, true).size(), 1u);
    app::Intent vis{app::IntentKind::SetVisibleIds};
    vis.ids = {"id0"};
    f.Send(vis);
    f.Send(app::IntentKind::SelectOuting, 2);
    f.Send(app::IntentKind::SetOnlyVisible, 1);
    ATF_REQUIRE_EQ(f.model.selected, -1);  // back to the overview
    ATF_REQUIRE_EQ(core::SelectOutings(f.model.outings, f.model.ListFilter(), core::SortKey::Date, true).size(), 0u);
    ATF_REQUIRE_EQ(core::SelectOutings(f.model.outings, f.model.MapFilter(), core::SortKey::Date, true).size(), 1u);
}

ATF_TEST_CASE_WITHOUT_HEAD(errors_queue_and_quit);
ATF_TEST_CASE_BODY(errors_queue_and_quit)
{
    Fixture f;
    app::Intent e{app::IntentKind::ReportError};
    e.text = "boom";
    f.Send(e);
    ATF_REQUIRE_EQ(f.model.errors.size(), 1u);
    f.Send(app::IntentKind::DismissError);
    ATF_REQUIRE(f.model.errors.empty());
    f.Send(app::IntentKind::Quit);
    ATF_REQUIRE(f.model.quit);
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, scan_and_overview_fill_the_model);
    ATF_ADD_TEST_CASE(tcs, selecting_loads_points_and_stats);
    ATF_ADD_TEST_CASE(tcs, filters_are_non_destructive);
    ATF_ADD_TEST_CASE(tcs, query_drives_list_and_map_filters);
    ATF_ADD_TEST_CASE(tcs, errors_queue_and_quit);
}
