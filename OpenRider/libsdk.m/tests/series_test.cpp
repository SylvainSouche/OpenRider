#include <atf-c++.hpp>

#include <openrider/sdk/series.h>

#include <cmath>
#include <limits>

namespace {

// A steady climb: 1 sample per second, 2 m/s horizontally, +0.25 m/s up
// (900 m/h, 12.5 % grade).
std::vector<or_point> Climb(int seconds)
{
    std::vector<or_point> pts;
    for (int i = 0; i <= seconds; ++i) {
        or_point p{};
        p.t_s = i;
        p.dist_m = 2.0 * i;
        p.alt_m = 1000.0 + 0.25 * i;
        p.speed_ms = 2.0;
        p.hr = 130;
        pts.push_back(p);
    }
    return pts;
}

bool Near(double a, double b) { return std::fabs(a - b) < 1e-6; }

} // namespace

ATF_TEST_CASE_WITHOUT_HEAD(vertical_speed_units);
ATF_TEST_CASE_BODY(vertical_speed_units)
{
    const auto pts = Climb(600);
    openrider::sdk::SeriesSpec s;
    s.kind = openrider::sdk::SeriesKind::VerticalSpeed;
    s.vspeed_window_s = 60.0;
    s.vspeed_unit = openrider::sdk::VSpeedUnit::MetresPerHour;
    ATF_REQUIRE(Near(openrider::sdk::ComputeSeries(pts, openrider::sdk::XAxis::Time, s).y[300], 900.0));
    s.vspeed_unit = openrider::sdk::VSpeedUnit::MetresPerMinute;
    ATF_REQUIRE(Near(openrider::sdk::ComputeSeries(pts, openrider::sdk::XAxis::Time, s).y[300], 15.0));
    s.vspeed_unit = openrider::sdk::VSpeedUnit::MetresPerSecond;
    ATF_REQUIRE(Near(openrider::sdk::ComputeSeries(pts, openrider::sdk::XAxis::Time, s).y[300], 0.25));
}

ATF_TEST_CASE_WITHOUT_HEAD(grade_over_distance_window);
ATF_TEST_CASE_BODY(grade_over_distance_window)
{
    const auto pts = Climb(600);
    for (double w : {100.0, 1000.0}) {
        const auto g = openrider::sdk::Grade(pts, w);
        ATF_REQUIRE(Near(g[300], 12.5));
    }
}

ATF_TEST_CASE_WITHOUT_HEAD(window_smooths_noise);
ATF_TEST_CASE_BODY(window_smooths_noise)
{
    auto pts = Climb(600);
    for (size_t i = 0; i < pts.size(); ++i)
        pts[i].alt_m += (i % 2 ? 1.0 : -1.0);  // +/- 1 m altimeter noise
    const double raw = (pts[301].alt_m - pts[300].alt_m) / 1.0;  // 2.25 m/s
    const double smooth = openrider::sdk::VerticalSpeed(pts, 300.0)[300];
    ATF_REQUIRE(std::fabs(raw - 0.25) > 1.0);
    ATF_REQUIRE(std::fabs(smooth - 0.25) < 0.01);
}

ATF_TEST_CASE_WITHOUT_HEAD(x_axis_time_and_distance);
ATF_TEST_CASE_BODY(x_axis_time_and_distance)
{
    const auto pts = Climb(120);
    ATF_REQUIRE(Near(openrider::sdk::XValues(pts, openrider::sdk::XAxis::Time)[120], 2.0));        // minutes
    ATF_REQUIRE(Near(openrider::sdk::XValues(pts, openrider::sdk::XAxis::Distance)[120], 0.24));   // km
}

ATF_TEST_CASE_WITHOUT_HEAD(recorded_series_in_display_units);
ATF_TEST_CASE_BODY(recorded_series_in_display_units)
{
    const auto pts = Climb(10);
    openrider::sdk::SeriesSpec s;
    s.kind = openrider::sdk::SeriesKind::Speed;
    ATF_REQUIRE(Near(openrider::sdk::ComputeSeries(pts, openrider::sdk::XAxis::Time, s).y[5], 7.2));  // km/h
    s.kind = openrider::sdk::SeriesKind::HeartRate;
    ATF_REQUIRE(Near(openrider::sdk::ComputeSeries(pts, openrider::sdk::XAxis::Time, s).y[5], 130.0));
}

ATF_TEST_CASE_WITHOUT_HEAD(missing_altitude_gives_gap);
ATF_TEST_CASE_BODY(missing_altitude_gives_gap)
{
    auto pts = Climb(10);
    for (auto &p : pts)
        p.alt_m = std::numeric_limits<double>::quiet_NaN();
    const auto v = openrider::sdk::VerticalSpeed(pts, 4.0);
    ATF_REQUIRE(std::isnan(v[5]));
}

ATF_TEST_CASE_WITHOUT_HEAD(histogram_vertical_speed_per_period);
ATF_TEST_CASE_BODY(histogram_vertical_speed_per_period)
{
    const auto pts = Climb(600);  // 10 minutes at 900 m/h
    openrider::sdk::SeriesSpec s;
    s.kind = openrider::sdk::SeriesKind::VerticalSpeed;
    s.type = openrider::sdk::GraphType::Histogram;
    s.vspeed_window_s = 60.0;
    s.vspeed_unit = openrider::sdk::VSpeedUnit::MetresPerHour;
    const auto h = openrider::sdk::HistogramSeries(pts, openrider::sdk::XAxis::Time, s);
    ATF_REQUIRE_EQ(h.x.size(), 11u);           // 10 one-minute bins + closing edge
    ATF_REQUIRE(Near(h.x[1], 1.0));            // edges in minutes
    for (size_t i = 0; i + 1 < h.y.size(); ++i)
        ATF_REQUIRE(Near(h.y[i], 900.0));
    ATF_REQUIRE_EQ(h.point_index[2], 120u);
}

ATF_TEST_CASE_WITHOUT_HEAD(histogram_on_distance_axis);
ATF_TEST_CASE_BODY(histogram_on_distance_axis)
{
    const auto pts = Climb(600);  // 1200 m at 2 m/s
    openrider::sdk::SeriesSpec s;
    s.kind = openrider::sdk::SeriesKind::Speed;
    const auto h = openrider::sdk::HistogramSeries(pts, openrider::sdk::XAxis::Distance, s);
    ATF_REQUIRE_EQ(h.x.size(), 13u);           // 12 bins of 100 m + closing edge
    ATF_REQUIRE(Near(h.y[0], 7.2));            // mean speed in km/h
    // Grade bins over its distance window, whatever the x axis.
    s.kind = openrider::sdk::SeriesKind::Grade;
    s.grade_window_m = 100.0;
    const auto g = openrider::sdk::HistogramSeries(pts, openrider::sdk::XAxis::Time, s);
    ATF_REQUIRE(Near(g.y[0], 12.5));
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, vertical_speed_units);
    ATF_ADD_TEST_CASE(tcs, grade_over_distance_window);
    ATF_ADD_TEST_CASE(tcs, window_smooths_noise);
    ATF_ADD_TEST_CASE(tcs, x_axis_time_and_distance);
    ATF_ADD_TEST_CASE(tcs, recorded_series_in_display_units);
    ATF_ADD_TEST_CASE(tcs, missing_altitude_gives_gap);
    ATF_ADD_TEST_CASE(tcs, histogram_vertical_speed_per_period);
    ATF_ADD_TEST_CASE(tcs, histogram_on_distance_axis);
}
