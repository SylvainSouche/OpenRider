#include <atf-c++.hpp>

#include "core/log.h"

ATF_TEST_CASE_WITHOUT_HEAD(name_has_date_and_time);
ATF_TEST_CASE_BODY(name_has_date_and_time)
{
    std::tm tm{};
    tm.tm_year = 2026 - 1900;
    tm.tm_mon = 9;
    tm.tm_mday = 12;
    tm.tm_hour = 16;
    tm.tm_min = 5;
    tm.tm_sec = 31;
    tm.tm_isdst = -1;
    const std::time_t t = std::mktime(&tm);
    ATF_REQUIRE_EQ(core::log_file_name(t), "OpenRider-2026-10-12_16-05-31.log");
    ATF_REQUIRE_EQ(core::log_file_time("OpenRider-2026-10-12_16-05-31.log"), t);
}

ATF_TEST_CASE_WITHOUT_HEAD(only_old_log_files_expire);
ATF_TEST_CASE_BODY(only_old_log_files_expire)
{
    const std::time_t now = core::log_file_time("OpenRider-2026-10-12_12-00-00.log");
    const std::vector<std::string> names = {
        "OpenRider-2026-10-01_09-00-00.log",  // 11 days old: expired
        "OpenRider-2026-10-03_13-00-00.log",  // under 10 days: kept
        "OpenRider-2026-10-12_08-00-00.log",  // today: kept
        "notes.txt", "OpenRider-garbage.log", ".", "..",
    };
    const auto old = core::expired_logs(names, now, core::kLogRetentionDays);
    ATF_REQUIRE_EQ(old.size(), 1u);
    ATF_REQUIRE_EQ(old[0], "OpenRider-2026-10-01_09-00-00.log");
}

ATF_TEST_CASE_WITHOUT_HEAD(unrelated_names_are_not_logs);
ATF_TEST_CASE_BODY(unrelated_names_are_not_logs)
{
    ATF_REQUIRE_EQ(core::log_file_time("OpenRider-2026-10-12_16-05-31.txt"), -1);
    ATF_REQUIRE_EQ(core::log_file_time("Other-2026-10-12_16-05-31.log"), -1);
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, name_has_date_and_time);
    ATF_ADD_TEST_CASE(tcs, only_old_log_files_expire);
    ATF_ADD_TEST_CASE(tcs, unrelated_names_are_not_logs);
}
