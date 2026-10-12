// GarminDB plugin tests: a tiny GarminDB-like tree is built in a temporary
// directory, then read back through the plugin's C ABI.

#include <atf-c++.hpp>

#include <openrider/input.h>
#include <openrider/plugin.h>
#include <sqlite3.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

extern "C" const or_plugin_info *openrider_plugin(void);

namespace {

std::string make_fixture()
{
    char tmpl[] = "/tmp/openrider-gdb-XXXXXX";
    std::string base = mkdtemp(tmpl);
    std::system(("mkdir -p " + base + "/DBs " + base + "/FitFiles/Activities").c_str());
    sqlite3 *db = nullptr;
    sqlite3_open((base + "/DBs/garmin_activities.db").c_str(), &db);
    sqlite3_exec(db,
        "CREATE TABLE activities (activity_id VARCHAR, name VARCHAR, sport VARCHAR,"
        " sub_sport VARCHAR, start_time DATETIME, elapsed_time TIME, moving_time TIME,"
        " distance FLOAT, ascent FLOAT, descent FLOAT, avg_speed FLOAT, max_speed FLOAT,"
        " avg_hr INTEGER, max_hr INTEGER, start_lat FLOAT, start_long FLOAT, avg_temperature FLOAT,"
        " min_temperature FLOAT, max_temperature FLOAT, calories INTEGER, training_effect FLOAT,"
        " anaerobic_training_effect FLOAT);"
        "CREATE TABLE activity_records (activity_id VARCHAR, record INTEGER,"
        " timestamp DATETIME, position_lat FLOAT, position_long FLOAT, distance FLOAT,"
        " cadence INTEGER, altitude FLOAT, hr INTEGER, rr FLOAT, speed FLOAT, temperature FLOAT);"
        "INSERT INTO activities VALUES ('1','DB name','running','trail',"
        " '2026-01-10 09:00:00.000000','01:00:00.000000','01:00:00.000000',"
        " 10.0,800,790,10.0,36.0,140,170,45.9,6.6,4.5,-2,9,900,3.1,1.2);"
        "INSERT INTO activities VALUES ('2','No JSON','hiking','generic',"
        " '2026-02-01 08:00:00.000000','02:30:00.000000','02:30:00.000000',"
        " 7.5,500,500,3.0,6.0,NULL,NULL,46.0,6.7,NULL,NULL,NULL,NULL,NULL,NULL);"
        "INSERT INTO activity_records VALUES ('1',0,'2026-01-10 09:00:00.000000',45.9,6.6,0.0,0,1000,100,NULL,0.0,5);"
        "INSERT INTO activity_records VALUES ('1',1,'2026-01-10 09:00:10.000000',45.9001,6.6,0.03,80,1002,110,NULL,10.8,5);",
        nullptr, nullptr, nullptr);
    sqlite3_close(db);
    FILE *f = std::fopen((base + "/FitFiles/Activities/activity_1.json").c_str(), "w");
    std::fputs("{\"activityName\":\"Ski tour\",\"activityType\":{\"typeKey\":\"backcountry_skiing\"},"
               "\"movingDuration\":3000.0,\"duration\":3600.0,\"distance\":10000.0,\"maxElevation\":2100.0,\"calories\":950.0}", f);
    std::fclose(f);
    return base;
}

const or_input_vtable *vtable()
{
    return static_cast<const or_input_vtable *>(openrider_plugin()->vtable);
}

int collect_outing(void *user, const or_outing *o, size_t, size_t)
{
    static_cast<std::vector<or_outing> *>(user)->push_back(*o);
    return 0;
}

int collect_point(void *user, const or_point *p)
{
    static_cast<std::vector<or_point> *>(user)->push_back(*p);
    return 0;
}

std::string config_for(const std::string &base)
{
    return "{\"base_dir\":\"" + base + "\"}";
}

} // namespace

ATF_TEST_CASE_WITHOUT_HEAD(descriptor);
ATF_TEST_CASE_BODY(descriptor)
{
    const or_plugin_info *info = openrider_plugin();
    ATF_REQUIRE_EQ(info->api_version, OR_PLUGIN_API_VERSION);
    ATF_REQUIRE_EQ(info->kind, static_cast<uint32_t>(OR_PLUGIN_INPUT));
    ATF_REQUIRE_EQ(std::string(info->name), "garmindb");
}

ATF_TEST_CASE_WITHOUT_HEAD(lists_outings_with_json_overlay);
ATF_TEST_CASE_BODY(lists_outings_with_json_overlay)
{
    const std::string base = make_fixture();
    char err[256] = "";
    void *h = vtable()->open(config_for(base).c_str(), err, sizeof err);
    if (h == nullptr)
        ATF_FAIL(std::string("open failed: ") + err);
    std::vector<or_outing> outings;
    ATF_REQUIRE_EQ(vtable()->list_outings(h, collect_outing, &outings, err, sizeof err), 0);
    vtable()->close(h);
    ATF_REQUIRE_EQ(outings.size(), 2u);

    // Outing 1: Garmin Connect JSON wins for type, name and moving time.
    ATF_REQUIRE_EQ(std::string(outings[0].source_type), "backcountry_skiing");
    ATF_REQUIRE_EQ(std::string(outings[0].name), "Ski tour");
    ATF_REQUIRE_EQ(std::string(outings[0].fit_sport), "running");
    ATF_REQUIRE(std::fabs(outings[0].moving_s - 3000.0) < 1e-6);
    ATF_REQUIRE(std::fabs(outings[0].max_speed_ms - 10.0) < 1e-6);   // 36 km/h
    ATF_REQUIRE(std::fabs(outings[0].max_alt_m - 2100.0) < 1e-6);    // JSON only
    ATF_REQUIRE(std::fabs(outings[0].calories - 950.0) < 1e-6);      // JSON over DB
    ATF_REQUIRE(std::fabs(outings[0].avg_temp_c - 4.5) < 1e-6);      // DB only
    ATF_REQUIRE(std::isnan(outings[1].calories));

    // Outing 2: no JSON, DB values converted to SI.
    ATF_REQUIRE_EQ(std::string(outings[1].source_type), "");
    ATF_REQUIRE(std::fabs(outings[1].distance_m - 7500.0) < 1e-6);
    ATF_REQUIRE(std::fabs(outings[1].duration_s - 9000.0) < 1e-6);
    ATF_REQUIRE_EQ(outings[1].avg_hr, -1);
}

ATF_TEST_CASE_WITHOUT_HEAD(loads_points_relative_time);
ATF_TEST_CASE_BODY(loads_points_relative_time)
{
    const std::string base = make_fixture();
    char err[256] = "";
    void *h = vtable()->open(config_for(base).c_str(), err, sizeof err);
    ATF_REQUIRE(h != nullptr);
    std::vector<or_point> pts;
    ATF_REQUIRE_EQ(vtable()->load_points(h, "1", collect_point, &pts, err, sizeof err), 0);
    vtable()->close(h);
    ATF_REQUIRE_EQ(pts.size(), 2u);
    ATF_REQUIRE(std::fabs(pts[1].t_s - 10.0) < 1e-6);
    ATF_REQUIRE(std::fabs(pts[1].dist_m - 30.0) < 1e-6);   // 0.03 km
    ATF_REQUIRE(std::fabs(pts[1].speed_ms - 3.0) < 1e-6);  // 10.8 km/h
    ATF_REQUIRE(std::isnan(pts[0].cadence) == false);
}

ATF_TEST_CASE_WITHOUT_HEAD(missing_database_reports_error);
ATF_TEST_CASE_BODY(missing_database_reports_error)
{
    char err[256] = "";
    void *h = vtable()->open("{\"base_dir\":\"/nonexistent/openrider\"}", err, sizeof err);
    ATF_REQUIRE(h == nullptr);
    ATF_REQUIRE(err[0] != '\0');
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, descriptor);
    ATF_ADD_TEST_CASE(tcs, lists_outings_with_json_overlay);
    ATF_ADD_TEST_CASE(tcs, loads_points_relative_time);
    ATF_ADD_TEST_CASE(tcs, missing_database_reports_error);
}
