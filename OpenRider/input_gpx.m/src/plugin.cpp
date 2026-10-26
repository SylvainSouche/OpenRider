// The GPX input plugin: the .gpx files of a folder (and its subfolders),
// one outing each, behind the input ABI (openrider/input.h).

#include <openrider/input.h>
#include <openrider/plugin.h>
#include <openrider/sdk/motion.h>
#include <openrider/sdk/stats.h>

#include "gpx/gpx_file.h"

#include <cJSON.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace {

// An open folder: the files found by the last listing, by outing id.
struct Handle {
    std::string dir;                           // the GPX folder
    std::mutex mutex;                          // guards paths
    std::map<std::string, std::string> paths;  // outing id -> file
};

void SetErr(char *err, size_t errlen, const std::string &message)
{
    if (err && errlen)
        std::snprintf(err, errlen, "%s", message.c_str());
}

void CopyStr(char *dst, size_t n, const std::string &src) { std::snprintf(dst, n, "%s", src.c_str()); }

std::string ReadFile(const std::string &path)
{
    std::ifstream in(path, std::ios::binary);
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}

// Stable id of a file: "gpx:" and a hash of its path within the folder
// (the same as long as the file stays where it is).
std::string IdOf(const std::string &relative)
{
    uint64_t hash = 1469598103934665603ull;  // FNV-1a
    for (unsigned char c : relative) {
        hash ^= c;
        hash *= 1099511628211ull;
    }
    char id[24];
    std::snprintf(id, sizeof id, "gpx:%016llx", static_cast<unsigned long long>(hash));
    return id;
}

// The .gpx files under `dir`, by id.
std::map<std::string, std::string> FilesIn(const std::string &dir)
{
    std::map<std::string, std::string> files;
    std::error_code ec;
    for (std::filesystem::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        const std::filesystem::path &path = it->path();
        std::string extension = path.extension().string();
        for (char &c : extension)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (extension != ".gpx" || !it->is_regular_file(ec))
            continue;
        files[IdOf(std::filesystem::relative(path, dir, ec).string())] = path.string();
    }
    return files;
}

void *GpxOpen(const char *config, char *err, size_t errlen)
{
    std::string dir;
    if (config && *config) {
        cJSON *root = cJSON_Parse(config);
        const cJSON *v = cJSON_GetObjectItemCaseSensitive(root, "dir");
        if (cJSON_IsString(v))
            dir = v->valuestring;
        cJSON_Delete(root);
    }
    if (dir.empty()) {
        SetErr(err, errlen, "gpx: no folder given (config {\"dir\": ...})");
        return nullptr;
    }
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) {
        SetErr(err, errlen, "gpx: no folder " + dir);
        return nullptr;
    }
    Handle *h = new Handle;
    h->dir = dir;
    return h;
}

void GpxClose(void *handle) { delete static_cast<Handle *>(handle); }

// The outing summary of a file.
or_outing Summary(const std::string &id, const std::string &path, const gpx::GpxFile &file)
{
    const double nan = std::nan("");
    or_outing o{};
    CopyStr(o.id, sizeof o.id, id);
    CopyStr(o.name, sizeof o.name, file.name.empty() ? std::filesystem::path(path).stem().string() : file.name);
    // A file without times (a route) is dated by its metadata or its name
    // (ParseGpx), else by a timestamp in the file's name, else by the
    // file's own modification time.
    double start = file.start_epoch_s;
    if (std::isnan(start))
        start = gpx::TimeInName(std::filesystem::path(path).stem().string());
    if (std::isnan(start)) {
        std::error_code ec;
        const auto written = std::filesystem::last_write_time(path, ec);
        if (!ec)
            start = static_cast<double>(std::chrono::duration_cast<std::chrono::seconds>(
                                            std::chrono::file_clock::to_sys(written).time_since_epoch())
                                            .count());
    }
    CopyStr(o.start_local, sizeof o.start_local, gpx::LocalTime(start));
    // The type as type tables key it: lower case, words joined by '_'.
    std::string type;
    for (char c : file.type)
        type += c == ' ' || c == '-' ? '_' : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    CopyStr(o.source_type, sizeof o.source_type, type);
    const openrider::sdk::ComputedStats stats = openrider::sdk::ComputeStats(file.points, openrider::sdk::MotionParams{});
    o.duration_s = stats.duration_s;
    o.moving_s = stats.moving_s;
    o.distance_m = stats.distance_m;
    o.ascent_m = stats.ascent_m;
    o.descent_m = stats.descent_m;
    o.avg_speed_ms = stats.avg_speed_ms;
    o.max_speed_ms = stats.max_speed_ms;
    o.min_alt_m = stats.min_alt_m;
    o.max_alt_m = stats.max_alt_m;
    double hr_sum = 0.0, hr_max = 0.0, temp_sum = 0.0, temp_min = nan, temp_max = nan;
    size_t hr_count = 0, temp_count = 0;
    o.start_lat = o.start_lon = nan;
    for (const or_point &p : file.points) {
        if (std::isnan(o.start_lat) && !std::isnan(p.lat)) {
            o.start_lat = p.lat;
            o.start_lon = p.lon;
        }
        if (!std::isnan(p.hr)) {
            hr_sum += p.hr;
            hr_max = std::max(hr_max, p.hr);
            ++hr_count;
        }
        if (!std::isnan(p.temp_c)) {
            temp_sum += p.temp_c;
            temp_min = std::isnan(temp_min) ? p.temp_c : std::min(temp_min, p.temp_c);
            temp_max = std::isnan(temp_max) ? p.temp_c : std::max(temp_max, p.temp_c);
            ++temp_count;
        }
    }
    o.avg_hr = hr_count ? static_cast<int32_t>(std::lround(hr_sum / hr_count)) : -1;
    o.max_hr = hr_count ? static_cast<int32_t>(std::lround(hr_max)) : -1;
    o.avg_temp_c = temp_count ? temp_sum / temp_count : nan;
    o.min_temp_c = temp_min;
    o.max_temp_c = temp_max;
    o.calories = o.aerobic_te = o.anaerobic_te = nan;
    // No times (a route): nothing measured against the clock (input.h:
    // duration_s NaN says so).
    if (!file.timed)
        o.duration_s = o.moving_s = o.avg_speed_ms = o.max_speed_ms = nan;
    return o;
}

int GpxList(void *handle, or_outing_cb cb, void *user, char *err, size_t errlen)
{
    Handle *h = static_cast<Handle *>(handle);
    if (!h) {
        SetErr(err, errlen, "gpx: no handle");
        return 1;
    }
    const std::map<std::string, std::string> files = FilesIn(h->dir);
    {
        std::lock_guard<std::mutex> lock(h->mutex);
        h->paths = files;
    }
    size_t index = 0;
    for (const auto &[id, path] : files) {
        const gpx::GpxFile file = gpx::ParseGpx(ReadFile(path));
        if (file.points.empty())
            continue;  // waypoints only: no outing
        const or_outing outing = Summary(id, path, file);
        if (cb(user, &outing, index++, files.size()) != 0)
            break;
    }
    return 0;
}

int GpxPoints(void *handle, const char *outing_id, or_point_cb cb, void *user, char *err, size_t errlen)
{
    Handle *h = static_cast<Handle *>(handle);
    if (!h || !outing_id) {
        SetErr(err, errlen, "gpx: missing arguments");
        return 1;
    }
    std::string path;
    {
        std::lock_guard<std::mutex> lock(h->mutex);
        auto found = h->paths.find(outing_id);
        if (found == h->paths.end()) {
            h->paths = FilesIn(h->dir);  // not listed yet: look again
            found = h->paths.find(outing_id);
        }
        if (found != h->paths.end())
            path = found->second;
    }
    if (path.empty()) {
        SetErr(err, errlen, std::string("gpx: no file for outing ") + outing_id);
        return 1;
    }
    for (const or_point &p : gpx::ParseGpx(ReadFile(path)).points)
        if (cb(user, &p) != 0)
            break;
    return 0;
}

const or_input_vtable kVtable = {sizeof(or_input_vtable), GpxOpen, GpxClose, GpxList, GpxPoints};
const or_plugin_info kInfo = {OR_PLUGIN_API_VERSION, OR_PLUGIN_API_MINOR, OR_PLUGIN_INPUT, OR_PLUGIN_THREAD_SAFE, "gpx",
                              "0.1", &kVtable};

} // namespace

extern "C" __attribute__((visibility("default"))) const or_plugin_info *openrider_plugin(void)
{
    return &kInfo;
}
