// Scaling with a library of thousands of real GPX tracks: scan, overview,
// segment networks, through the application's classes and plugins.
//
// Each stage is timed against a budget in units of a calibration workload
// (as perf.tst), and reports its peak memory. The tracks are those
// testdata/cc-gpx-scale.tsv.gz lists, fetched by bmake fetch-scale-data;
// without them every case is skipped. When fewer than kTargetTracks are
// real, the library is topped up with derived tracks: real timed tracks
// shifted by whole days and a little in place, named "derived ...", written
// once into derived/ beside them. Scanning and loading run on all of them;
// segment networks on the real ones only (copies would make overlaps up).
//
// Budgets are three to four times the ratios measured on 10,000 real tracks
// (Apple M-series, 10 cores, 2026-10-24): scan 5,400 units (44 s, 56 MB),
// overview 6,300 (51 s, 320 MB, 11.4 M points kept), segment networks
// 20,600 and 19,800 for the heatmap and matching plugins (169 s and 162 s,
// 1.4 GB, 535 groups of 5,366 outings, the largest 232) -- one core: the
// network builder takes the groups one after another. Each case's Kyua
// timeout leaves room for its whole budget.

#include <atf-c++.hpp>

#include "core/input_set.h"
#include "core/library.h"
#include "core/network_builder.h"
#include "core/overview_loader.h"
#include "core/plugins.h"
#include "core/type_catalog.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <string>
#include <sys/resource.h>
#include <sys/stat.h>
#include <thread>
#include <vector>

namespace {

// Tracks below this number: the scale cases are skipped.
#ifndef SCALE_MINIMUM
#define SCALE_MINIMUM 200
#endif
constexpr size_t kMinimumTracks = SCALE_MINIMUM;
// The library's size, derived tracks included.
constexpr size_t kTargetTracks = 10000;

volatile double g_sink = 0.0;  // keeps results alive

// Wall time of `work` (milliseconds).
double Time(const std::function<void()> &work)
{
    const auto start = std::chrono::steady_clock::now();
    work();
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

// The calibration workload: 2 million rounds of trigonometry, the best of
// five (milliseconds); the same as perf.tst's.
double Calibration()
{
    static const double time = [] {
        double best = 1e300;
        for (int run = 0; run < 5; ++run)
            best = std::min(best, Time([] {
                double sum = 0.0;
                for (int i = 0; i < 2000000; ++i) {
                    const double x = i * 1e-6;
                    sum += std::sin(x) * std::cos(x * 0.5) + std::sqrt(x + 1.0);
                }
                g_sink = sum;
            }));
        return best;
    }();
    return time;
}

// Peak resident memory of the process so far (MB).
double PeakMegabytes()
{
    rusage usage{};
    getrusage(RUSAGE_SELF, &usage);
#ifdef __APPLE__
    return usage.ru_maxrss / 1048576.0;  // bytes
#else
    return usage.ru_maxrss / 1024.0;     // kilobytes
#endif
}

// Reports a stage and checks it against `budget` calibration units.
void Check(const std::string &name, double milliseconds, double budget, const std::string &detail)
{
    const double ratio = milliseconds / Calibration();
    std::printf("scale %-26s %10.1f ms %8.1f units (budget %6.0f) %7.0f MB peak  %s\n", name.c_str(), milliseconds,
                ratio, budget, PeakMegabytes(), detail.c_str());
    if (ratio > budget)
        ATF_FAIL(name + ": " + std::to_string(ratio) + " calibration units, budget " + std::to_string(budget));
}

// The .gpx files of `directory` (not its subdirectories), sorted.
std::vector<std::string> GpxFiles(const std::string &directory)
{
    std::vector<std::string> files;
    if (DIR *dir = opendir(directory.c_str())) {
        while (dirent *entry = readdir(dir)) {
            const std::string name = entry->d_name;
            if (name.size() > 4 && name.compare(name.size() - 4, 4, ".gpx") == 0)
                files.push_back(directory + "/" + name);
        }
        closedir(dir);
    }
    std::sort(files.begin(), files.end());
    return files;
}

// The number of real tracks fetched.
size_t TrackFiles() { return GpxFiles(SCALE_DIR).size(); }

std::string ReadAll(const std::string &path)
{
    std::ifstream file(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

// `text` with every <time> moved by `seconds` (ISO 8601 UTC written back)
// and every lat/lon attribute moved by `degrees`, every name prefixed with
// "derived <n>" (a file without one is named after its file, "derived-...").
std::string Derived(const std::string &text, double seconds, double degrees, size_t n)
{
    std::string out;
    out.reserve(text.size() + 64);
    size_t at = 0;
    while (at < text.size()) {
        const size_t next = text.find_first_of("<l", at);
        if (next == std::string::npos) {
            out.append(text, at, std::string::npos);
            break;
        }
        out.append(text, at, next - at);
        at = next;
        if (text.compare(at, 6, "<time>") == 0) {
            const size_t end = text.find("</time>", at);
            std::tm tm{};
            int year, month, day, hour, minute;
            double second;
            if (end != std::string::npos &&
                std::sscanf(text.c_str() + at + 6, "%d-%d-%dT%d:%d:%lf", &year, &month, &day, &hour, &minute, &second) == 6) {
                tm.tm_year = year - 1900;
                tm.tm_mon = month - 1;
                tm.tm_mday = day;
                tm.tm_hour = hour;
                tm.tm_min = minute;
                const std::time_t shifted = timegm(&tm) + static_cast<std::time_t>(second + seconds);
                std::tm out_tm{};
                gmtime_r(&shifted, &out_tm);
                char stamp[32];
                std::strftime(stamp, sizeof stamp, "%Y-%m-%dT%H:%M:%SZ", &out_tm);
                out += "<time>";
                out += stamp;
                at = end;
                continue;
            }
        } else if (text.compare(at, 6, "<name>") == 0) {
            // Every name (metadata's, track's): the outing is named after
            // one of them, which one depends on the file.
            out += "<name>derived " + std::to_string(n) + " ";
            at += 6;
            continue;
        } else if (text.compare(at, 5, "lat=\"") == 0 || text.compare(at, 5, "lon=\"") == 0 ||
                   text.compare(at, 5, "lat='") == 0 || text.compare(at, 5, "lon='") == 0) {
            char *end = nullptr;
            const double value = std::strtod(text.c_str() + at + 5, &end);
            if (end && end != text.c_str() + at + 5) {
                char number[32];
                std::snprintf(number, sizeof number, "%.7f", value + degrees);
                out.append(text, at, 5);
                out += number;
                at = static_cast<size_t>(end - text.c_str());
                continue;
            }
        }
        out += text[at++];
    }
    return out;
}

// Tops the library up to kTargetTracks with derived tracks, once: real
// timed tracks, each copy shifted by one more day and 0.001 degree.
void EnsureDerived()
{
    const std::string derived = std::string(SCALE_DIR) + "/derived";
    const std::vector<std::string> real = GpxFiles(SCALE_DIR);
    if (real.size() >= kTargetTracks)
        return;
    const size_t wanted = kTargetTracks - real.size();
    // The derived files of this version (the marker records it).
    const std::string marker = derived + "/version";
    if (GpxFiles(derived).size() >= wanted && ReadAll(marker) == "2\n")
        return;
    std::vector<std::string> timed;
    for (const std::string &path : real)
        if (ReadAll(path).find("<time>") != std::string::npos)
            timed.push_back(path);
    if (timed.empty())
        return;
    mkdir(derived.c_str(), 0755);
    for (size_t n = 0; n < wanted; ++n) {
        const size_t round = n / timed.size() + 1;
        const std::string text = Derived(ReadAll(timed[n % timed.size()]), round * 86400.0, round * 0.001, n);
        char name[64];
        std::snprintf(name, sizeof name, "/derived-%05zu.gpx", n);
        std::ofstream(derived + name, std::ios::binary) << text;
    }
    std::ofstream(marker) << "2\n";
}

// The GPX plugin opened on the scale directory, as the application opens it.
struct Library {
    core::InputSet inputs;
    core::TypeCatalog catalog;
    std::vector<core::Outing> outings;
    double scan_ms = 0.0;

    Library()
    {
        const core::PluginScan scan = core::LoadPlugins({PLUGINS_DIR}, OR_PLUGIN_INPUT);
        for (const core::LoadedPlugin &plugin : scan.loaded)
            if (std::string(plugin.info->name) == "gpx") {
                const auto *vtable = static_cast<const or_input_vtable *>(plugin.info->vtable);
                char error[256] = "";
                void *handle = vtable->open((std::string("{\"dir\":\"") + SCALE_DIR + "\"}").c_str(), error, sizeof error);
                if (!handle)
                    ATF_FAIL(std::string("gpx plugin: ") + error);
                inputs.Add({vtable, handle, "gpx", "gpx", false, SCALE_DIR});
            }
        if (inputs.Empty())
            ATF_FAIL("no gpx plugin in " PLUGINS_DIR);
        scan_ms = Time([&] { outings = core::ScanSource(inputs, catalog, 0, {}).outings; });
    }
};

// Skips the case without enough real tracks; tops the library up.
void RequireTracks()
{
    const size_t files = TrackFiles();
    if (files < kMinimumTracks)
        ATF_SKIP("only " + std::to_string(files) + " tracks in testdata/cc-gpx-scale: run bmake fetch-scale-data");
    EnsureDerived();
}

// Whether an outing is one of the derived copies.
bool IsDerived(const core::Outing &outing) { return std::strncmp(outing.raw.name, "derived", 7) == 0; }

} // namespace

ATF_TEST_CASE(library_scan);
ATF_TEST_CASE_HEAD(library_scan) { set_md_var("timeout", "900"); }
ATF_TEST_CASE_BODY(library_scan)
{
    // Every file read and summarised (statistics computed from its points),
    // duplicates detected.
    RequireTracks();
    const Library library;
    size_t timed = 0, derived = 0;
    for (const core::Outing &outing : library.outings) {
        timed += outing.Timed() ? 1 : 0;
        derived += IsDerived(outing) ? 1 : 0;
    }
    Check("library scan", library.scan_ms, 16000.0,
          std::to_string(library.outings.size()) + " outings (" + std::to_string(timed) + " timed, " +
              std::to_string(derived) + " derived), " +
              std::to_string(library.scan_ms / std::max<size_t>(1, library.outings.size())).substr(0, 5) +
              " ms an outing");
    ATF_REQUIRE(library.outings.size() >= kMinimumTracks / 2);
}

ATF_TEST_CASE(overview_load);
ATF_TEST_CASE_HEAD(overview_load) { set_md_var("timeout", "1200"); }
ATF_TEST_CASE_BODY(overview_load)
{
    // Every outing's points loaded and thinned for the overview map.
    RequireTracks();
    const Library library;
    std::vector<std::string> ids;
    for (const core::Outing &outing : library.outings)
        ids.push_back(outing.raw.id);
    core::OverviewLoader loader;
    std::vector<openrider::sdk::OverviewTrack> tracks;
    const double ms = Time([&] {
        loader.Start(&library.inputs, ids);
        while (loader.Loading())
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        loader.Take(tracks);
    });
    size_t points = 0;
    for (const openrider::sdk::OverviewTrack &track : tracks)
        points += track.lat.size();
    Check("overview load", ms, 25000.0, std::to_string(tracks.size()) + " tracks, " + std::to_string(points) + " points kept");
    ATF_REQUIRE_EQ(tracks.size(), ids.size());
}

ATF_TEST_CASE(segment_networks);
ATF_TEST_CASE_HEAD(segment_networks) { set_md_var("timeout", "3600"); }
ATF_TEST_CASE_BODY(segment_networks)
{
    // The real timed outings grouped as by area (start within the same 0.5
    // degree cell), then each segments plugin builds the networks of every
    // group, as the application does with its sports.
    RequireTracks();
    const Library library;
    std::map<std::pair<int, int>, core::TrackGroup> cells;
    for (const core::Outing &outing : library.outings) {
        if (!outing.Timed() || std::isnan(outing.raw.start_lat) || IsDerived(outing))
            continue;
        core::TrackGroup &group = cells[{static_cast<int>(std::floor(outing.raw.start_lat * 2)),
                                         static_cast<int>(std::floor(outing.raw.start_lon * 2))}];
        group.sport = "cell";
        group.ids.push_back(outing.raw.id);
    }
    std::vector<core::TrackGroup> groups;
    size_t largest = 0, outings = 0;
    for (auto &[cell, group] : cells)
        if (group.ids.size() >= 2) {
            largest = std::max(largest, group.ids.size());
            outings += group.ids.size();
            groups.push_back(group);
        }
    const core::PluginScan scan = core::LoadPlugins({PLUGINS_DIR}, OR_PLUGIN_SEGMENTS);
    ATF_REQUIRE(!scan.loaded.empty());
    for (const core::LoadedPlugin &plugin : scan.loaded) {
        const auto *vtable = static_cast<const or_segments_vtable *>(plugin.info->vtable);
        std::vector<double> values;
        for (size_t p = 0; p < vtable->param_count; ++p)
            values.push_back(vtable->params[p].default_value);
        core::NetworkBuilder builder;
        std::vector<core::SegmentGroup> networks;
        const double ms = Time([&] {
            builder.Start(groups, vtable, values, &library.inputs);
            while (!builder.Take(networks))
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
        });
        size_t segments = 0;
        for (const core::SegmentGroup &network : networks)
            segments += network.network.segments.size();
        Check(std::string("segments: ") + plugin.info->name, ms, 62000.0,
              std::to_string(groups.size()) + " groups of " + std::to_string(outings) + " outings (largest " +
                  std::to_string(largest) + "), " + std::to_string(segments) + " segments");
        ATF_REQUIRE_EQ(builder.Error(), "");
    }
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, library_scan);
    ATF_ADD_TEST_CASE(tcs, overview_load);
    ATF_ADD_TEST_CASE(tcs, segment_networks);
}
