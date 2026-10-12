#include <atf-c++.hpp>

#include "core/plugins.h"

namespace {
int dummy;
}

ATF_TEST_CASE_WITHOUT_HEAD(accepts_matching_descriptor);
ATF_TEST_CASE_BODY(accepts_matching_descriptor)
{
    const or_plugin_info ok{OR_PLUGIN_API_VERSION, OR_PLUGIN_INPUT, "x", "1", &dummy};
    ATF_REQUIRE_EQ(core::plugin_check(&ok, OR_PLUGIN_INPUT), "");
}

ATF_TEST_CASE_WITHOUT_HEAD(refuses_other_api_version);
ATF_TEST_CASE_BODY(refuses_other_api_version)
{
    const or_plugin_info old{OR_PLUGIN_API_VERSION + 1, OR_PLUGIN_INPUT, "x", "1", &dummy};
    ATF_REQUIRE(core::plugin_check(&old, OR_PLUGIN_INPUT).find("API version") != std::string::npos);
}

ATF_TEST_CASE_WITHOUT_HEAD(refuses_wrong_kind_or_incomplete);
ATF_TEST_CASE_BODY(refuses_wrong_kind_or_incomplete)
{
    const or_plugin_info tiles{OR_PLUGIN_API_VERSION, OR_PLUGIN_TILES, "x", "1", &dummy};
    ATF_REQUIRE(!core::plugin_check(&tiles, OR_PLUGIN_INPUT).empty());
    const or_plugin_info empty{OR_PLUGIN_API_VERSION, OR_PLUGIN_INPUT, "x", "1", nullptr};
    ATF_REQUIRE(!core::plugin_check(&empty, OR_PLUGIN_INPUT).empty());
    ATF_REQUIRE(!core::plugin_check(nullptr, OR_PLUGIN_INPUT).empty());
}

ATF_TEST_CASE_WITHOUT_HEAD(kind_directories);
ATF_TEST_CASE_BODY(kind_directories)
{
    ATF_REQUIRE_EQ(std::string(core::plugin_kind_dir(OR_PLUGIN_INPUT)), "input");
    ATF_REQUIRE_EQ(std::string(core::plugin_kind_dir(OR_PLUGIN_FILTER)), "filters");
    ATF_REQUIRE_EQ(std::string(core::plugin_kind_dir(OR_PLUGIN_TILES)), "tiles");
    ATF_REQUIRE_EQ(std::string(core::plugin_kind_dir(OR_PLUGIN_WORKFLOW)), "workflows");
}

ATF_TEST_CASE_WITHOUT_HEAD(missing_directory_loads_nothing);
ATF_TEST_CASE_BODY(missing_directory_loads_nothing)
{
    const core::PluginScan s = core::load_plugins({"/nonexistent/openrider"}, OR_PLUGIN_INPUT);
    ATF_REQUIRE(s.loaded.empty());
    ATF_REQUIRE(s.problems.empty());
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, accepts_matching_descriptor);
    ATF_ADD_TEST_CASE(tcs, refuses_other_api_version);
    ATF_ADD_TEST_CASE(tcs, refuses_wrong_kind_or_incomplete);
    ATF_ADD_TEST_CASE(tcs, kind_directories);
    ATF_ADD_TEST_CASE(tcs, missing_directory_loads_nothing);
}
