// The GPX input plugin on files written by the test.

#include <atf-c++.hpp>

#include <openrider/input.h>
#include <openrider/plugin.h>
#include <openrider/sdk/stats.h>

#include "gpx/gpx_file.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

extern "C" const or_plugin_info *openrider_plugin(void);

namespace {

// A Garmin-style GPX 1.1 export: name, type, points with heart rate.
const char *const kGarminGpx = R"(<?xml version="1.0" encoding="UTF-8"?>
<gpx creator="Garmin Connect" version="1.1" xmlns:ns3="http://www.garmin.com/xmlschemas/TrackPointExtension/v1">
  <metadata><time>2022-11-20T11:06:48.000Z</time></metadata>
  <trk>
    <name>Saint-Lary Trail</name>
    <type>trail_running</type>
    <trkseg>
      <trkpt lat="42.8146679" lon="0.3252855"><ele>852.4</ele><time>2022-11-20T11:06:48.000Z</time>
        <extensions><ns3:TrackPointExtension><ns3:hr>79</ns3:hr><ns3:cad>0</ns3:cad></ns3:TrackPointExtension></extensions></trkpt>
      <trkpt lat="42.8147352" lon="0.3253158"><ele>853.0</ele><time>2022-11-20T11:06:52.000Z</time>
        <extensions><ns3:TrackPointExtension><ns3:hr>81</ns3:hr></ns3:TrackPointExtension></extensions></trkpt>
      <trkpt lat="42.8148020" lon="0.3253500"><ele>853.6</ele><time>2022-11-20T11:06:55+00:00</time></trkpt>
    </trkseg>
  </trk>
</gpx>
)";

std::string TempDir()
{
    char tmpl[] = "/tmp/openrider-gpx-XXXXXX";
    return mkdtemp(tmpl);
}

void Write(const std::string &path, const std::string &text) { std::ofstream(path) << text; }

const or_input_vtable *Vt() { return static_cast<const or_input_vtable *>(openrider_plugin()->vtable); }

} // namespace

ATF_TEST_CASE_WITHOUT_HEAD(parses_points_and_extensions);
ATF_TEST_CASE_BODY(parses_points_and_extensions)
{
    const gpx::GpxFile file = gpx::ParseGpx(kGarminGpx);
    ATF_REQUIRE_EQ(file.name, "Saint-Lary Trail");
    ATF_REQUIRE_EQ(file.type, "trail_running");
    ATF_REQUIRE_EQ(file.points.size(), 3u);
    ATF_REQUIRE_EQ(file.points[0].hr, 79.0);
    ATF_REQUIRE_EQ(file.points[1].t_s, 4.0);
    ATF_REQUIRE_EQ(file.points[2].t_s, 7.0);  // "+00:00" read as UTC too
    ATF_REQUIRE(std::isnan(file.points[2].hr));
    ATF_REQUIRE(std::fabs(file.points[2].alt_m - 853.6) < 1e-9);
    ATF_REQUIRE(file.points[2].dist_m > 10.0 && file.points[2].dist_m < 30.0);
    ATF_REQUIRE_EQ(gpx::ParseTime("2022-11-20T11:06:48Z"), 1668942408.0);
    ATF_REQUIRE_EQ(gpx::ParseTime("2022-11-20T12:06:48+01:00"), 1668942408.0);
}

ATF_TEST_CASE_WITHOUT_HEAD(lists_and_loads_a_folder);
ATF_TEST_CASE_BODY(lists_and_loads_a_folder)
{
    const std::string dir = TempDir();
    Write(dir + "/trail.gpx", kGarminGpx);
    std::system(("mkdir -p '" + dir + "/sub'").c_str());
    Write(dir + "/sub/Other.GPX", kGarminGpx);
    Write(dir + "/notes.txt", "not a track");
    Write(dir + "/route.gpx", "<gpx><rte><rtept lat=\"1\" lon=\"2\"/><rtept lat=\"1.001\" lon=\"2\"/></rte></gpx>");
    Write(dir + "/places.gpx", "<gpx><wpt lat=\"1\" lon=\"2\"/></gpx>");  // waypoints only: no outing
    char err[256] = "";
    void *h = Vt()->open(("{\"dir\":\"" + dir + "\"}").c_str(), err, sizeof err);
    ATF_REQUIRE(h != nullptr);
    std::vector<or_outing> outings;
    ATF_REQUIRE_EQ(Vt()->list_outings(h, [](void *u, const or_outing *o, size_t, size_t) {
        static_cast<std::vector<or_outing> *>(u)->push_back(*o);
        return 0;
    }, &outings, err, sizeof err), 0);
    // Two tracks and the route (an outing without times).
    ATF_REQUIRE_EQ(outings.size(), 3u);
    size_t tracks = 0, routes = 0;
    for (const or_outing &outing : outings) {
        ATF_REQUIRE(std::string(outing.id).rfind("gpx:", 0) == 0);
        if (std::isnan(outing.duration_s)) {
            ++routes;
            ATF_REQUIRE(outing.distance_m > 100.0);
            ATF_REQUIRE(std::isnan(outing.avg_speed_ms) && std::isnan(outing.moving_s));
            ATF_REQUIRE(std::string(outing.start_local).size() == 19);  // dated by the file
        } else {
            ++tracks;
            ATF_REQUIRE_EQ(std::string(outing.source_type), "trail_running");
            ATF_REQUIRE_EQ(outing.avg_hr, 80);
            ATF_REQUIRE(outing.distance_m > 10.0);
        }
    }
    ATF_REQUIRE_EQ(tracks, 2u);
    ATF_REQUIRE_EQ(routes, 1u);
    while (std::isnan(outings[1].duration_s))
        std::swap(outings[1], outings[2]);  // a track's points below
    std::vector<or_point> points;
    ATF_REQUIRE_EQ(Vt()->load_points(h, outings[1].id, [](void *u, const or_point *p) {
        static_cast<std::vector<or_point> *>(u)->push_back(*p);
        return 0;
    }, &points, err, sizeof err), 0);
    ATF_REQUIRE_EQ(points.size(), 3u);
    ATF_REQUIRE(Vt()->load_points(h, "gpx:0000000000000000", [](void *, const or_point *) { return 0; }, nullptr, err,
                                  sizeof err) != 0);
    Vt()->close(h);
    ATF_REQUIRE(Vt()->open(nullptr, err, sizeof err) == nullptr);  // no folder given
}

ATF_TEST_CASE_WITHOUT_HEAD(route_without_times);
ATF_TEST_CASE_BODY(route_without_times)
{
    // A ride exported by the Suunto app as a route: <rtept> with elevation,
    // no time anywhere; the date is in the name.
    const gpx::GpxFile file = gpx::ParseGpx(
        "<?xml version=\"1.0\"?><gpx creator=\"Suunto app\" version=\"1.1\"><metadata>"
        "<name>suuntoapp-Cycling-2026-10-23T12-00-43Z</name></metadata><rte><name>suuntoapp-Cycling-2026-10-23T12-00-43Z</name>"
        "<rtept lat=\"43.169017\" lon=\"6.523312\"><ele>20.2</ele></rtept>"
        "<rtept lat=\"43.169\" lon=\"6.523292\"><ele>20.4</ele></rtept>"
        "<rtept lat=\"43.168985\" lon=\"6.523277\"><ele>21.2</ele></rtept></rte></gpx>");
    ATF_REQUIRE(file.route);
    ATF_REQUIRE(!file.timed);
    ATF_REQUIRE_EQ(file.points.size(), 3u);
    for (const or_point &p : file.points)
        ATF_REQUIRE(std::isnan(p.t_s));  // never invented
    ATF_REQUIRE(file.points[2].dist_m > 3.0 && file.points[2].dist_m < 5.0);
    ATF_REQUIRE_EQ(file.name, "suuntoapp-Cycling-2026-10-23T12-00-43Z");
    ATF_REQUIRE_EQ(file.type, "cycling");  // from the Suunto name
    ATF_REQUIRE_EQ(gpx::ParseGpx("<gpx><metadata><name>suuntoapp-TrailRunning-2026</name></metadata><rte>"
                                 "<rtept lat=\"1\" lon=\"2\"/></rte></gpx>").type,
                   "trail_running");
    ATF_REQUIRE_EQ(file.start_epoch_s, gpx::ParseTime("2026-10-23T12:00:43Z"));
    // Timestamps in names, as exports write them.
    ATF_REQUIRE_EQ(gpx::TimeInName("ride 2026-10-23T12:00:43Z"), gpx::ParseTime("2026-10-23T12:00:43Z"));
    ATF_REQUIRE_EQ(gpx::TimeInName("2026-10-23_12-00-43.gpx"), gpx::ParseTime("2026-10-23T12:00:43Z"));
    ATF_REQUIRE(std::isnan(gpx::TimeInName("Morning ride")));
    ATF_REQUIRE(std::isnan(gpx::TimeInName("2026-13-40T99-00-00")));
}

ATF_TEST_CASE_WITHOUT_HEAD(self_closing_points);
ATF_TEST_CASE_BODY(self_closing_points)
{
    // Points written <trkpt .../> one after the other: none is skipped.
    const gpx::GpxFile file = gpx::ParseGpx("<gpx><trk><trkseg><trkpt lat=\"1\" lon=\"2\"/><trkpt lat=\"1.001\" lon=\"2\"/>"
                                            "<trkpt lat=\"1.002\" lon=\"2\"/></trkseg></trk></gpx>");
    ATF_REQUIRE_EQ(file.points.size(), 3u);
    ATF_REQUIRE(std::fabs(file.points[2].dist_m - 222.39) < 0.1);
}

ATF_TEST_CASE_WITHOUT_HEAD(times_never_go_back);
ATF_TEST_CASE_BODY(times_never_go_back)
{
    // A 12-hour clock without AM/PM: 12:59:50 then 01:00:10 is 13:00:10.
    // A stray earlier time is dropped and interpolated.
    const gpx::GpxFile file = gpx::ParseGpx(
        "<gpx><trk><trkseg>"
        "<trkpt lat=\"45.000\" lon=\"6.0\"><time>2026-10-23T12:59:50Z</time></trkpt>"
        "<trkpt lat=\"45.001\" lon=\"6.0\"><time>2026-10-23T01:00:10Z</time></trkpt>"
        "<trkpt lat=\"45.002\" lon=\"6.0\"><time>2026-10-23T09:00:00Z</time></trkpt>"
        "<trkpt lat=\"45.003\" lon=\"6.0\"><time>2026-10-23T01:00:50Z</time></trkpt>"
        "</trkseg></trk></gpx>");
    ATF_REQUIRE(file.timed);
    ATF_REQUIRE_EQ(file.points[0].t_s, 0.0);
    ATF_REQUIRE_EQ(file.points[1].t_s, 20.0);
    ATF_REQUIRE(std::fabs(file.points[2].t_s - 40.0) < 0.1);  // stray 09:00 dropped, interpolated
    ATF_REQUIRE_EQ(file.points[3].t_s, 60.0);
}

ATF_TEST_CASE_WITHOUT_HEAD(missing_times_interpolated);
ATF_TEST_CASE_BODY(missing_times_interpolated)
{
    // Times missing between timed points follow the distance; before the
    // first, the first's.
    const gpx::GpxFile file = gpx::ParseGpx(
        "<gpx><trk><trkseg>"
        "<trkpt lat=\"45.0\" lon=\"6.0\"></trkpt>"
        "<trkpt lat=\"45.001\" lon=\"6.0\"><time>2026-10-23T10:00:00Z</time></trkpt>"
        "<trkpt lat=\"45.002\" lon=\"6.0\"></trkpt>"
        "<trkpt lat=\"45.004\" lon=\"6.0\"></trkpt>"
        "<trkpt lat=\"45.005\" lon=\"6.0\"><time>2026-10-23T10:01:00Z</time></trkpt>"
        "</trkseg></trk></gpx>");
    ATF_REQUIRE(file.timed && !file.route);
    ATF_REQUIRE_EQ(file.points[0].t_s, 0.0);
    ATF_REQUIRE_EQ(file.points[1].t_s, 0.0);
    ATF_REQUIRE(std::fabs(file.points[2].t_s - 15.0) < 0.1);  // a quarter of the way
    ATF_REQUIRE(std::fabs(file.points[3].t_s - 45.0) < 0.1);
    ATF_REQUIRE_EQ(file.points[4].t_s, 60.0);
}

ATF_TEST_CASE_WITHOUT_HEAD(published_tracks);
ATF_TEST_CASE_BODY(published_tracks)
{
    // Real GPX files published on the web (testdata/cc-gpx.tsv: recorded
    // logs, 12-hour clocks, generated times, routes without times) read as
    // when the manifest was written. Skipped until bmake fetch-test-data
    // has downloaded them.
    std::ifstream manifest(GPX_CORPUS_MANIFEST);
    ATF_REQUIRE(manifest.good());
    std::string line;
    int checked = 0, missing = 0;
    std::string failures;
    while (std::getline(manifest, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::vector<std::string> column;
        for (size_t start = 0, end; start <= line.size(); start = end + 1) {
            end = line.find('\t', start);
            if (end == std::string::npos)
                end = line.size();
            column.push_back(line.substr(start, end - start));
        }
        ATF_REQUIRE(column.size() >= 14);
        std::ifstream file(std::string(GPX_CORPUS_DIR) + "/" + column[0] + ".gpx", std::ios::binary);
        if (!file) {
            ++missing;
            continue;
        }
        std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        const gpx::GpxFile gpx_file = gpx::ParseGpx(text);
        const openrider::sdk::ComputedStats stats =
            openrider::sdk::ComputeStats(gpx_file.points, openrider::sdk::MotionParams{});
        const double distance = gpx_file.points.empty() ? 0.0 : gpx_file.points.back().dist_m;
        auto close = [](double actual, const std::string &expected, double tolerance) {
            const double value = std::strtod(expected.c_str(), nullptr);
            return std::isnan(value) ? std::isnan(actual) : std::fabs(actual - value) <= tolerance;
        };
        const bool same = std::to_string(gpx_file.points.size()) == column[8] &&
                          (gpx_file.route ? "1" : "0") == column[9] && (gpx_file.timed ? "1" : "0") == column[10] &&
                          close(distance, column[11], 0.5) && close(stats.duration_s, column[12], 0.5) &&
                          close(stats.ascent_m, column[13], 1.0);
        if (!same)
            failures += " " + column[0] + " (" + column[6] + ")";
        ++checked;
    }
    if (checked == 0)
        ATF_SKIP("no published track fetched: run bmake fetch-test-data");
    if (!failures.empty())
        ATF_FAIL("read differently from testdata/cc-gpx.tsv:" + failures);
    if (missing > 0)
        std::fprintf(stderr, "%d tracks missing: run bmake fetch-test-data\n", missing);
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, published_tracks);
    ATF_ADD_TEST_CASE(tcs, route_without_times);
    ATF_ADD_TEST_CASE(tcs, missing_times_interpolated);
    ATF_ADD_TEST_CASE(tcs, times_never_go_back);
    ATF_ADD_TEST_CASE(tcs, self_closing_points);
    ATF_ADD_TEST_CASE(tcs, parses_points_and_extensions);
    ATF_ADD_TEST_CASE(tcs, lists_and_loads_a_folder);
}
