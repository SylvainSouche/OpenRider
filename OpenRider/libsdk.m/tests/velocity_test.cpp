// Velocity of recorded points.

#include <atf-c++.hpp>

#include <openrider/sdk/velocity.h>

#include <cmath>

namespace {

constexpr double kLat0 = 45.9, kLon0 = 6.6;
constexpr double kKx = 111320.0 * 0.69591, kKy = 110540.0;

// Points one a second, going east at `speed` m/s, with +/- `wander` m of
// north-south GPS wander, recorded at `speed`.
std::vector<or_point> East(double speed, double wander, int count)
{
    std::vector<or_point> points;
    for (int i = 0; i < count; ++i) {
        or_point p{};
        p.t_s = i;
        p.lon = kLon0 + speed * i / kKx;
        p.lat = kLat0 + (i % 2 ? wander : -wander) / kKy;
        p.speed_ms = speed;
        points.push_back(p);
    }
    return points;
}

} // namespace

ATF_TEST_CASE_WITHOUT_HEAD(recorded_speed_along_the_way);
ATF_TEST_CASE_BODY(recorded_speed_along_the_way)
{
    const openrider::sdk::LocalProjection projection(kLon0, kLat0);
    // 3 m/s east, wandering +/-1 m across: the direction looks 5 m of path
    // ahead and behind, so the wander barely turns it.
    const auto velocities = openrider::sdk::PointVelocities(East(3.0, 1.0, 40), projection);
    for (size_t i = 5; i < 35; ++i) {
        ATF_REQUIRE(std::fabs(std::hypot(velocities[i].x, velocities[i].y) - 3.0) < 1e-9);
        ATF_REQUIRE(velocities[i].x > 2.5);
    }
}

ATF_TEST_CASE_WITHOUT_HEAD(stopped_and_unlocated_points);
ATF_TEST_CASE_BODY(stopped_and_unlocated_points)
{
    const openrider::sdk::LocalProjection projection(kLon0, kLat0);
    std::vector<or_point> points = East(3.0, 0.0, 10);
    points[4].speed_ms = 0.0;  // recorded stopped
    points[6].lat = points[6].lon = std::nan("");
    points[8].speed_ms = std::nan("");  // no recorded speed: from the positions, 3 m/s
    const auto velocities = openrider::sdk::PointVelocities(points, projection);
    ATF_REQUIRE_EQ(velocities[4].x, 0.0);
    ATF_REQUIRE_EQ(velocities[4].y, 0.0);
    ATF_REQUIRE(std::isnan(velocities[6].x));
    ATF_REQUIRE(std::fabs(velocities[8].x - 3.0) < 0.05);
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, recorded_speed_along_the_way);
    ATF_ADD_TEST_CASE(tcs, stopped_and_unlocated_points);
}
