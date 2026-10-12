// GarminDB input plugin: reads GarminDB's activity database and the Garmin
// Connect summary JSON files next to the FIT files. Read-only: the database
// is opened with mode=ro and nothing is ever written to the GarminDB tree.

#include <openrider/input.h>
#include <openrider/plugin.h>

#include <cJSON.h>
#include <sqlite3.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

struct Handle {
    std::string base_dir;      // e.g. ~/HealthData
    sqlite3 *db = nullptr;     // garmin_activities.db, read-only
};

void set_err(char *err, size_t errlen, const char *fmt, const char *arg)
{
    if (err && errlen)
        std::snprintf(err, errlen, fmt, arg ? arg : "");
}

void copy_str(char *dst, size_t n, const char *src)
{
    if (!src)
        src = "";
    std::snprintf(dst, n, "%s", src);
}

double col_double(sqlite3_stmt *st, int i)
{
    return sqlite3_column_type(st, i) == SQLITE_NULL ? NAN : sqlite3_column_double(st, i);
}

const char *col_text(sqlite3_stmt *st, int i)
{
    const unsigned char *t = sqlite3_column_text(st, i);
    return t ? reinterpret_cast<const char *>(t) : "";
}

// "HH:MM:SS[.ffffff]" -> seconds; NaN when empty.
double parse_hms(const char *s)
{
    int h = 0, m = 0;
    double sec = 0.0;
    if (!s || !*s || std::sscanf(s, "%d:%d:%lf", &h, &m, &sec) != 3)
        return NAN;
    return h * 3600.0 + m * 60.0 + sec;
}

// "YYYY-MM-DD HH:MM:SS[.ffffff]" -> seconds since an arbitrary epoch
// (days-from-civil); only differences are used, so the time zone is moot.
double parse_timestamp(const char *s)
{
    int y, mo, d, h, mi;
    double sec;
    if (!s || std::sscanf(s, "%d-%d-%d %d:%d:%lf", &y, &mo, &d, &h, &mi, &sec) != 6)
        return NAN;
    y -= mo <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const int yoe = y - era * 400;
    const int doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    const double days = era * 146097.0 + doe - 719468.0;
    return days * 86400.0 + h * 3600.0 + mi * 60.0 + sec;
}

double json_number(const cJSON *obj, const char *key)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsNumber(v) ? v->valuedouble : NAN;
}

std::string read_file(const std::string &path)
{
    std::string out;
    FILE *f = std::fopen(path.c_str(), "rb");
    if (!f)
        return out;
    char buf[8192];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0)
        out.append(buf, n);
    std::fclose(f);
    return out;
}

// Overlay Garmin Connect's summary (activity_<id>.json) on the DB row: it
// carries the user's type corrections and the real moving time.
void apply_summary_json(const Handle &h, or_outing &o)
{
    const std::string path = h.base_dir + "/FitFiles/Activities/activity_" + o.id + ".json";
    const std::string text = read_file(path);
    if (text.empty())
        return;
    cJSON *root = cJSON_Parse(text.c_str());
    if (!root)
        return;
    const cJSON *type = cJSON_GetObjectItemCaseSensitive(root, "activityType");
    const cJSON *key = cJSON_GetObjectItemCaseSensitive(type, "typeKey");
    if (cJSON_IsString(key))
        copy_str(o.source_type, sizeof o.source_type, key->valuestring);
    const cJSON *name = cJSON_GetObjectItemCaseSensitive(root, "activityName");
    if (cJSON_IsString(name) && name->valuestring[0])
        copy_str(o.name, sizeof o.name, name->valuestring);

    struct { const char *key; double *dst; } nums[] = {
        {"duration", &o.duration_s},      {"movingDuration", &o.moving_s},
        {"distance", &o.distance_m},      {"elevationGain", &o.ascent_m},
        {"elevationLoss", &o.descent_m},  {"averageSpeed", &o.avg_speed_ms},
        {"maxSpeed", &o.max_speed_ms},    {"minElevation", &o.min_alt_m},
        {"maxElevation", &o.max_alt_m},   {"averageTemperature", &o.avg_temp_c},
        {"minTemperature", &o.min_temp_c}, {"maxTemperature", &o.max_temp_c},
        {"calories", &o.calories},        {"aerobicTrainingEffect", &o.aerobic_te},
        {"anaerobicTrainingEffect", &o.anaerobic_te},
    };
    for (auto &n : nums) {
        const double v = json_number(root, n.key);
        if (!std::isnan(v))
            *n.dst = v;
    }
    const double ahr = json_number(root, "averageHR");
    const double mhr = json_number(root, "maxHR");
    if (!std::isnan(ahr))
        o.avg_hr = static_cast<int32_t>(std::lround(ahr));
    if (!std::isnan(mhr))
        o.max_hr = static_cast<int32_t>(std::lround(mhr));
    cJSON_Delete(root);
}

std::string config_base_dir(const char *config)
{
    std::string dir;
    if (config && *config) {
        cJSON *root = cJSON_Parse(config);
        const cJSON *v = cJSON_GetObjectItemCaseSensitive(root, "base_dir");
        if (cJSON_IsString(v))
            dir = v->valuestring;
        cJSON_Delete(root);
    }
    if (dir.empty()) {
        const char *home = std::getenv("HOME");
        dir = std::string(home ? home : "") + "/HealthData";
    }
    return dir;
}

void *gdb_open(const char *config, char *err, size_t errlen)
{
    Handle *h = new Handle;
    h->base_dir = config_base_dir(config);
    const std::string uri = "file:" + h->base_dir + "/DBs/garmin_activities.db?mode=ro";
    if (sqlite3_open_v2(uri.c_str(), &h->db, SQLITE_OPEN_READONLY | SQLITE_OPEN_URI,
                        nullptr) != SQLITE_OK) {
        set_err(err, errlen, "cannot open GarminDB activities database: %s",
                h->db ? sqlite3_errmsg(h->db) : "out of memory");
        sqlite3_close(h->db);
        delete h;
        return nullptr;
    }
    return h;
}

void gdb_close(void *handle)
{
    Handle *h = static_cast<Handle *>(handle);
    if (!h)
        return;
    sqlite3_close(h->db);
    delete h;
}

int gdb_list(void *handle, or_outing_cb cb, void *user, char *err, size_t errlen)
{
    Handle *h = static_cast<Handle *>(handle);
    size_t total = 0;
    sqlite3_stmt *st = nullptr;
    if (sqlite3_prepare_v2(h->db, "SELECT count(*) FROM activities", -1, &st, nullptr) == SQLITE_OK &&
        sqlite3_step(st) == SQLITE_ROW)
        total = static_cast<size_t>(sqlite3_column_int64(st, 0));
    sqlite3_finalize(st);

    const char *sql =
        "SELECT activity_id, name, sport, sub_sport, start_time, elapsed_time,"
        " moving_time, distance, ascent, descent, avg_speed, max_speed,"
        " avg_hr, max_hr, start_lat, start_long, avg_temperature,"
        " min_temperature, max_temperature, calories, training_effect,"
        " anaerobic_training_effect"
        " FROM activities ORDER BY start_time";
    if (sqlite3_prepare_v2(h->db, sql, -1, &st, nullptr) != SQLITE_OK) {
        set_err(err, errlen, "cannot read activities: %s", sqlite3_errmsg(h->db));
        return 1;
    }
    size_t index = 0;
    int rc;
    while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
        or_outing o;
        std::memset(&o, 0, sizeof o);
        copy_str(o.id, sizeof o.id, col_text(st, 0));
        copy_str(o.name, sizeof o.name, col_text(st, 1));
        copy_str(o.fit_sport, sizeof o.fit_sport, col_text(st, 2));
        copy_str(o.fit_sub_sport, sizeof o.fit_sub_sport, col_text(st, 3));
        copy_str(o.start_local, sizeof o.start_local, col_text(st, 4));
        o.duration_s = parse_hms(col_text(st, 5));
        o.moving_s = parse_hms(col_text(st, 6));
        o.distance_m = col_double(st, 7) * 1000.0;         // DB: km
        o.ascent_m = col_double(st, 8);
        o.descent_m = col_double(st, 9);
        o.avg_speed_ms = col_double(st, 10) / 3.6;         // DB: km/h
        o.max_speed_ms = col_double(st, 11) / 3.6;
        o.avg_hr = sqlite3_column_type(st, 12) == SQLITE_NULL ? -1 : sqlite3_column_int(st, 12);
        o.max_hr = sqlite3_column_type(st, 13) == SQLITE_NULL ? -1 : sqlite3_column_int(st, 13);
        o.start_lat = col_double(st, 14);
        o.start_lon = col_double(st, 15);
        o.min_alt_m = NAN;  // not in the activities table; JSON only
        o.max_alt_m = NAN;
        o.avg_temp_c = col_double(st, 16);
        o.min_temp_c = col_double(st, 17);
        o.max_temp_c = col_double(st, 18);
        o.calories = col_double(st, 19);
        o.aerobic_te = col_double(st, 20);
        o.anaerobic_te = col_double(st, 21);
        apply_summary_json(*h, o);
        if (cb(user, &o, index++, total) != 0)
            break;
    }
    const bool failed = rc != SQLITE_ROW && rc != SQLITE_DONE;
    if (failed)
        set_err(err, errlen, "error reading activities: %s", sqlite3_errmsg(h->db));
    sqlite3_finalize(st);
    return failed ? 1 : 0;
}

int gdb_points(void *handle, const char *outing_id, or_point_cb cb, void *user,
               char *err, size_t errlen)
{
    Handle *h = static_cast<Handle *>(handle);
    const char *sql =
        "SELECT timestamp, position_lat, position_long, altitude, distance,"
        " speed, hr, cadence, temperature"
        " FROM activity_records WHERE activity_id = ? ORDER BY record";
    sqlite3_stmt *st = nullptr;
    if (sqlite3_prepare_v2(h->db, sql, -1, &st, nullptr) != SQLITE_OK) {
        set_err(err, errlen, "cannot read records: %s", sqlite3_errmsg(h->db));
        return 1;
    }
    sqlite3_bind_text(st, 1, outing_id, -1, SQLITE_TRANSIENT);
    double t0 = NAN;
    int rc;
    while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
        const double t = parse_timestamp(col_text(st, 0));
        if (std::isnan(t0))
            t0 = t;
        or_point p;
        p.t_s = t - t0;
        p.lat = col_double(st, 1);
        p.lon = col_double(st, 2);
        p.alt_m = col_double(st, 3);
        p.dist_m = col_double(st, 4) * 1000.0;             // DB: km
        p.speed_ms = col_double(st, 5) / 3.6;              // DB: km/h
        p.hr = col_double(st, 6);
        p.cadence = col_double(st, 7);
        p.temp_c = col_double(st, 8);
        if (cb(user, &p) != 0)
            break;
    }
    const bool failed = rc != SQLITE_ROW && rc != SQLITE_DONE;
    if (failed)
        set_err(err, errlen, "error reading records: %s", sqlite3_errmsg(h->db));
    sqlite3_finalize(st);
    return failed ? 1 : 0;
}

const or_input_vtable kVtable = {gdb_open, gdb_close, gdb_list, gdb_points};

const or_plugin_info kInfo = {OR_PLUGIN_API_VERSION, OR_PLUGIN_INPUT, "garmindb",
                              "0.1", &kVtable};

} // namespace

// @impl 0f87-6acd-24ce-494d
extern "C" __attribute__((visibility("default"))) const or_plugin_info *openrider_plugin(void)
{
    return &kInfo;
}
