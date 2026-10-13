#include <atf-c++.hpp>

#include "app/dispatch.h"
#include "app/graphs.h"

#include <cmath>

namespace {

app::AppModel ModelWithPoints(const char *id)
{
    app::AppModel m;
    m.points_id = id;
    for (int i = 0; i < 600; ++i) {  // 10 min, 2 m/s, +0.25 m/s
        or_point p{};
        p.t_s = i;
        p.dist_m = 2.0 * i;
        p.alt_m = 1000.0 + 0.25 * i;
        p.speed_ms = 2.0;
        m.points.push_back(p);
    }
    return m;
}

app::Intent Intent(app::IntentKind k, int i = 0, double x = 0.0)
{
    app::Intent in{k};
    in.i = i;
    in.x = x;
    return in;
}

} // namespace

ATF_TEST_CASE_WITHOUT_HEAD(main_layout);
ATF_TEST_CASE_BODY(main_layout)
{
    app::MainViewController c;
    c.Handle(Intent(app::IntentKind::ResizeList, 0, 300.0));
    ATF_REQUIRE_EQ(c.ViewModel().list_width, 300.0f);
    app::Intent map{app::IntentKind::ResizeMap};
    map.y = 250.0;
    c.Handle(map);
    ATF_REQUIRE_EQ(c.ViewModel().map_height, 250.0f);
    c.Handle(Intent(app::IntentKind::ShowAbout, 1));
    ATF_REQUIRE(c.ViewModel().show_about);
}

ATF_TEST_CASE_WITHOUT_HEAD(list_sorting);
ATF_TEST_CASE_BODY(list_sorting)
{
    app::ListViewController c;
    ATF_REQUIRE(c.ViewModel().sort_key == core::SortKey::Date && !c.ViewModel().ascending);
    c.Handle(Intent(app::IntentKind::SortBy, static_cast<int>(core::SortKey::Distance)));
    c.Handle(Intent(app::IntentKind::ToggleSortDirection));
    ATF_REQUIRE(c.ViewModel().sort_key == core::SortKey::Distance && c.ViewModel().ascending);
}

ATF_TEST_CASE_WITHOUT_HEAD(graph_stack_keeps_one);
ATF_TEST_CASE_BODY(graph_stack_keeps_one)
{
    const app::AppModel m = ModelWithPoints("a");
    app::OutingViewController c;
    c.Handle(Intent(app::IntentKind::RemoveGraph, 0), m);
    ATF_REQUIRE_EQ(c.ViewModel().graphs.size(), 1u);
    c.Handle(Intent(app::IntentKind::AddGraph), m);
    app::Intent spec = Intent(app::IntentKind::SetGraphSpec, 1);
    spec.spec.kind = openrider::sdk::SeriesKind::VerticalSpeed;
    c.Handle(spec, m);
    ATF_REQUIRE(c.ViewModel().graphs[1].kind == openrider::sdk::SeriesKind::VerticalSpeed);
    c.Handle(Intent(app::IntentKind::RemoveGraph, 0), m);
    ATF_REQUIRE_EQ(c.ViewModel().graphs.size(), 1u);
}

ATF_TEST_CASE_WITHOUT_HEAD(hover_finds_the_point);
ATF_TEST_CASE_BODY(hover_finds_the_point)
{
    const app::AppModel m = ModelWithPoints("a");
    app::OutingViewController c;
    c.Tick(m);
    c.Handle(Intent(app::IntentKind::GraphHover, 0, 2.5), m);  // 2.5 min on the time axis
    ATF_REQUIRE_EQ(c.ViewModel().hover_index, 150);
    // Histogram: the bar holding x = 2.5 min starts at minute 2 (point 120).
    app::Intent spec = Intent(app::IntentKind::SetGraphSpec, 0);
    spec.spec.kind = openrider::sdk::SeriesKind::VerticalSpeed;
    spec.spec.type = openrider::sdk::GraphType::Histogram;
    c.Handle(spec, m);
    c.Handle(Intent(app::IntentKind::GraphHover, 0, 2.5), m);
    ATF_REQUIRE_EQ(c.ViewModel().hover_index, 120);
    // A new outing clears the hover.
    c.Tick(ModelWithPoints("b"));
    ATF_REQUIRE_EQ(c.ViewModel().hover_index, -1);
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, main_layout);
    ATF_ADD_TEST_CASE(tcs, list_sorting);
    ATF_ADD_TEST_CASE(tcs, graph_stack_keeps_one);
    ATF_ADD_TEST_CASE(tcs, hover_finds_the_point);
}
