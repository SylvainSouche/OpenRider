#include <atf-c++.hpp>

#include "core/plugin_thread.h"
#include "core/plugins.h"

#include <openrider/input.h>

#include <algorithm>
#include <chrono>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

namespace {
const or_input_vtable g_input{sizeof(or_input_vtable), nullptr, nullptr, nullptr, nullptr};
const size_t g_short_table = sizeof(size_t);
const void *const g_dummy_table = &g_input;
}

ATF_TEST_CASE_WITHOUT_HEAD(accepts_matching_descriptor);
ATF_TEST_CASE_BODY(accepts_matching_descriptor)
{
    const or_plugin_info ok{OR_PLUGIN_API_VERSION, OR_PLUGIN_API_MINOR, OR_PLUGIN_INPUT, 0, "x", "1", g_dummy_table};
    ATF_REQUIRE_EQ(core::PluginCheck(&ok, OR_PLUGIN_INPUT), "");
}

ATF_TEST_CASE_WITHOUT_HEAD(refuses_other_api_version);
ATF_TEST_CASE_BODY(refuses_other_api_version)
{
    const or_plugin_info old{OR_PLUGIN_API_VERSION + 1, 0, OR_PLUGIN_INPUT, 0, "x", "1", g_dummy_table};
    ATF_REQUIRE(core::PluginCheck(&old, OR_PLUGIN_INPUT).find("API version") != std::string::npos);
}

ATF_TEST_CASE_WITHOUT_HEAD(minor_versions_and_table_sizes);
ATF_TEST_CASE_BODY(minor_versions_and_table_sizes)
{
    const or_plugin_info older{OR_PLUGIN_API_VERSION, 0, OR_PLUGIN_INPUT, OR_PLUGIN_THREAD_SAFE, "x", "1", g_dummy_table};
    ATF_REQUIRE_EQ(core::PluginCheck(&older, OR_PLUGIN_INPUT), "");
    const or_plugin_info newer{OR_PLUGIN_API_VERSION, OR_PLUGIN_API_MINOR + 1, OR_PLUGIN_INPUT, 0, "x", "1", g_dummy_table};
    ATF_REQUIRE(core::PluginCheck(&newer, OR_PLUGIN_INPUT).find("newer") != std::string::npos);
    const or_plugin_info short_table{OR_PLUGIN_API_VERSION, OR_PLUGIN_API_MINOR, OR_PLUGIN_INPUT, 0, "x", "1", &g_short_table};
    ATF_REQUIRE(core::PluginCheck(&short_table, OR_PLUGIN_INPUT).find("function table") != std::string::npos);
}

ATF_TEST_CASE_WITHOUT_HEAD(thread_safe_plugin_called_in_place);
ATF_TEST_CASE_BODY(thread_safe_plugin_called_in_place)
{
    const int table = 0;  // stands for a thread-safe plugin's function table
    std::thread::id ran;
    core::CallPlugin(&table, [&] { ran = std::this_thread::get_id(); });
    ATF_REQUIRE(ran == std::this_thread::get_id());
}

ATF_TEST_CASE_WITHOUT_HEAD(other_plugin_called_from_one_thread);
ATF_TEST_CASE_BODY(other_plugin_called_from_one_thread)
{
    const int table = 0;  // stands for a plugin without OR_PLUGIN_THREAD_SAFE
    core::SerialisePlugin(&table);
    std::mutex mutex;
    std::set<std::thread::id> threads;
    int inside = 0, most_inside = 0, calls = 0;
    auto call = [&] {
        {
            std::lock_guard<std::mutex> lock(mutex);
            threads.insert(std::this_thread::get_id());
            most_inside = std::max(most_inside, ++inside);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        std::lock_guard<std::mutex> lock(mutex);
        --inside;
        ++calls;
    };
    std::vector<std::thread> callers;
    for (int t = 0; t < 4; ++t)
        callers.emplace_back([&] {
            for (int k = 0; k < 10; ++k)
                core::CallPlugin(&table, call);
        });
    for (std::thread &caller : callers)
        caller.join();
    ATF_REQUIRE_EQ(calls, 40);
    ATF_REQUIRE_EQ(threads.size(), 1u);
    ATF_REQUIRE(*threads.begin() != std::this_thread::get_id());
    ATF_REQUIRE_EQ(most_inside, 1);
    // A call made from within a call runs at once, on the same thread.
    bool nested = false;
    core::CallPlugin(&table, [&] { core::CallPlugin(&table, [&] { nested = true; }); });
    ATF_REQUIRE(nested);
}

ATF_TEST_CASE_WITHOUT_HEAD(refuses_wrong_kind_or_incomplete);
ATF_TEST_CASE_BODY(refuses_wrong_kind_or_incomplete)
{
    const or_plugin_info tiles{OR_PLUGIN_API_VERSION, OR_PLUGIN_API_MINOR, OR_PLUGIN_TILES, 0, "x", "1", g_dummy_table};
    ATF_REQUIRE(!core::PluginCheck(&tiles, OR_PLUGIN_INPUT).empty());
    const or_plugin_info empty{OR_PLUGIN_API_VERSION, OR_PLUGIN_API_MINOR, OR_PLUGIN_INPUT, 0, "x", "1", nullptr};
    ATF_REQUIRE(!core::PluginCheck(&empty, OR_PLUGIN_INPUT).empty());
    ATF_REQUIRE(!core::PluginCheck(nullptr, OR_PLUGIN_INPUT).empty());
}

ATF_TEST_CASE_WITHOUT_HEAD(kind_directories);
ATF_TEST_CASE_BODY(kind_directories)
{
    ATF_REQUIRE_EQ(std::string(core::PluginKindDir(OR_PLUGIN_INPUT)), "input");
    ATF_REQUIRE_EQ(std::string(core::PluginKindDir(OR_PLUGIN_FILTER)), "filters");
    ATF_REQUIRE_EQ(std::string(core::PluginKindDir(OR_PLUGIN_TILES)), "tiles");
    ATF_REQUIRE_EQ(std::string(core::PluginKindDir(OR_PLUGIN_WORKFLOW)), "workflows");
    ATF_REQUIRE_EQ(std::string(core::PluginKindDir(OR_PLUGIN_SEGMENTS)), "segments");
    for (uint32_t kind = OR_PLUGIN_INPUT; kind <= OR_PLUGIN_SEGMENTS; ++kind)
        ATF_REQUIRE_EQ(core::PluginKindFromDir(core::PluginKindDir(kind)), kind);
    ATF_REQUIRE_EQ(core::PluginKindFromDir("filter"), 0u);
    ATF_REQUIRE_EQ(core::PluginKindFromDir(""), 0u);
}

ATF_TEST_CASE_WITHOUT_HEAD(missing_library_fails_to_open);
ATF_TEST_CASE_BODY(missing_library_fails_to_open)
{
    const core::PluginLoad load = core::LoadPlugin("/nonexistent/openrider/plugin.so", OR_PLUGIN_INPUT);
    ATF_REQUIRE(load.stage == core::PluginStage::kOpen);
    ATF_REQUIRE(!load.problem.empty());
    ATF_REQUIRE(load.plugin.handle == nullptr);
}

ATF_TEST_CASE_WITHOUT_HEAD(missing_directory_loads_nothing);
ATF_TEST_CASE_BODY(missing_directory_loads_nothing)
{
    const core::PluginScan s = core::LoadPlugins({"/nonexistent/openrider"}, OR_PLUGIN_INPUT);
    ATF_REQUIRE(s.loaded.empty());
    ATF_REQUIRE(s.problems.empty());
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, accepts_matching_descriptor);
    ATF_ADD_TEST_CASE(tcs, refuses_other_api_version);
    ATF_ADD_TEST_CASE(tcs, minor_versions_and_table_sizes);
    ATF_ADD_TEST_CASE(tcs, thread_safe_plugin_called_in_place);
    ATF_ADD_TEST_CASE(tcs, other_plugin_called_from_one_thread);
    ATF_ADD_TEST_CASE(tcs, refuses_wrong_kind_or_incomplete);
    ATF_ADD_TEST_CASE(tcs, kind_directories);
    ATF_ADD_TEST_CASE(tcs, missing_directory_loads_nothing);
    ATF_ADD_TEST_CASE(tcs, missing_library_fails_to_open);
}
