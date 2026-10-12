#include <atf-c++.hpp>

#include "maptiles/policy.h"

#include <cmath>

ATF_TEST_CASE_WITHOUT_HEAD(delay_backoff_and_decay);
ATF_TEST_CASE_BODY(delay_backoff_and_decay)
{
    maptiles::DelayPolicy d;
    ATF_REQUIRE_EQ(d.delay_ms, 100.0);
    d.on_error();
    ATF_REQUIRE_EQ(d.delay_ms, 700.0);   // 2 x 100 + 500
    d.on_error();
    ATF_REQUIRE_EQ(d.delay_ms, 1900.0);  // 2 x 700 + 500
    d.on_success();
    ATF_REQUIRE(std::fabs(d.delay_ms - 1710.0) < 1e-9);  // -10 %
    for (int i = 0; i < 100; ++i)
        d.on_success();
    ATF_REQUIRE_EQ(d.delay_ms, 100.0);   // never below the floor
}

ATF_TEST_CASE_WITHOUT_HEAD(round_robin_over_servers);
ATF_TEST_CASE_BODY(round_robin_over_servers)
{
    maptiles::ServiceState s(3);
    ATF_REQUIRE_EQ(s.pick_server(0), 0);
    ATF_REQUIRE_EQ(s.pick_server(0), 1);
    ATF_REQUIRE_EQ(s.pick_server(0), 2);
    ATF_REQUIRE_EQ(s.pick_server(0), 0);
}

ATF_TEST_CASE_WITHOUT_HEAD(failing_mirror_set_aside_while_others_work);
ATF_TEST_CASE_BODY(failing_mirror_set_aside_while_others_work)
{
    maptiles::ServiceState s(3);
    s.report(0, true, 0);
    s.report(2, true, 0);
    s.report(1, false, 10);  // b fails while a and c work
    for (int i = 0; i < 6; ++i)
        ATF_REQUIRE(s.pick_server(20) != 1);
    ATF_REQUIRE(!s.unavailable());
    // After 5 minutes it is used again.
    bool used = false;
    for (int i = 0; i < 3; ++i)
        used = used || s.pick_server(10 + 300) == 1;
    ATF_REQUIRE(used);
}

ATF_TEST_CASE_WITHOUT_HEAD(all_failing_makes_service_unavailable);
ATF_TEST_CASE_BODY(all_failing_makes_service_unavailable)
{
    maptiles::ServiceState s(3);
    s.report(0, false, 0);
    s.report(1, false, 0);
    ATF_REQUIRE(!s.unavailable());
    s.report(2, false, 0);
    ATF_REQUIRE(s.unavailable());
    s.report(1, true, 1);  // any success brings it back
    ATF_REQUIRE(!s.unavailable());
}

ATF_TEST_CASE_WITHOUT_HEAD(single_server_needs_three_failures);
ATF_TEST_CASE_BODY(single_server_needs_three_failures)
{
    maptiles::ServiceState s(1);
    s.report(0, false, 0);
    s.report(0, false, 0);
    ATF_REQUIRE(!s.unavailable());
    ATF_REQUIRE_EQ(s.pick_server(1), 0);  // never set aside: no other server
    s.report(0, false, 0);
    ATF_REQUIRE(s.unavailable());
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, delay_backoff_and_decay);
    ATF_ADD_TEST_CASE(tcs, round_robin_over_servers);
    ATF_ADD_TEST_CASE(tcs, failing_mirror_set_aside_while_others_work);
    ATF_ADD_TEST_CASE(tcs, all_failing_makes_service_unavailable);
    ATF_ADD_TEST_CASE(tcs, single_server_needs_three_failures);
}
