// Where the application finds itself and keeps the user's files.

#include <atf-c++.hpp>

#include "core/paths.h"

#include <cstdlib>
#include <string>
#include <sys/stat.h>

ATF_TEST_CASE_WITHOUT_HEAD(executable_dir_holds_this_test);
ATF_TEST_CASE_BODY(executable_dir_holds_this_test)
{
    // The directory of the running binary: this test program is in it
    // (found by the kernel or the loader, not the working directory).
    const std::string dir = core::ExecutableDir();
    ATF_REQUIRE(dir != ".");
    struct stat info;
    ATF_REQUIRE(stat((dir + "/paths_test").c_str(), &info) == 0 || stat((dir + "/paths_test.exe").c_str(), &info) == 0);
}

ATF_TEST_CASE_WITHOUT_HEAD(user_directories);
ATF_TEST_CASE_BODY(user_directories)
{
#if defined(__APPLE__)
    setenv("HOME", "/Users/someone", 1);
    ATF_REQUIRE_EQ(core::UserSupportDir(), "/Users/someone/Library/Application Support/OpenRider");
    ATF_REQUIRE_EQ(core::UserCacheDir(), "/Users/someone/Library/Caches/OpenRider");
    ATF_REQUIRE_EQ(core::UserLogsDir(), "/Users/someone/Library/Logs/OpenRider");
#elif !defined(_WIN32)
    // XDG: the variables when absolute, else the specification's defaults.
    setenv("HOME", "/home/someone", 1);
    unsetenv("XDG_DATA_HOME");
    unsetenv("XDG_CACHE_HOME");
    setenv("XDG_STATE_HOME", "relative/state", 1);  // ignored: not absolute
    ATF_REQUIRE_EQ(core::UserSupportDir(), "/home/someone/.local/share/OpenRider");
    ATF_REQUIRE_EQ(core::UserCacheDir(), "/home/someone/.cache/OpenRider");
    ATF_REQUIRE_EQ(core::UserLogsDir(), "/home/someone/.local/state/OpenRider/logs");
    setenv("XDG_DATA_HOME", "/data", 1);
    setenv("XDG_CACHE_HOME", "/cache", 1);
    setenv("XDG_STATE_HOME", "/state", 1);
    ATF_REQUIRE_EQ(core::UserSupportDir(), "/data/OpenRider");
    ATF_REQUIRE_EQ(core::UserPluginsDir(), "/data/OpenRider/plugins");
    ATF_REQUIRE_EQ(core::UserCacheDir(), "/cache/OpenRider");
    ATF_REQUIRE_EQ(core::UserLogsDir(), "/state/OpenRider/logs");
#endif
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, executable_dir_holds_this_test);
    ATF_ADD_TEST_CASE(tcs, user_directories);
}
