// The GPX input plugin on files written by the test.

#include <atf-c++.hpp>

#include <openrider/input.h>
#include <openrider/plugin.h>

#include "gpx/gpx_file.h"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

extern "C" const or_plugin_info *openrider_plugin(void);

namespace {

// A Garmin-style GPX 1.1 export: name, type, points with heart rate.
const char *kGarminGpx = R"(<?xml version="1.0" encoding="UTF-8"?>
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
    Write(dir + "/route.gpx", "<gpx><rte><rtept lat=\"1\" lon=\"2\"/></rte></gpx>");  // no track: no outing
    char err[256] = "";
    void *h = Vt()->open(("{\"dir\":\"" + dir + "\"}").c_str(), err, sizeof err);
    ATF_REQUIRE(h != nullptr);
    std::vector<or_outing> outings;
    ATF_REQUIRE_EQ(Vt()->list_outings(h, [](void *u, const or_outing *o, size_t, size_t) {
        static_cast<std::vector<or_outing> *>(u)->push_back(*o);
        return 0;
    }, &outings, err, sizeof err), 0);
    ATF_REQUIRE_EQ(outings.size(), 2u);
    ATF_REQUIRE(std::string(outings[0].id).rfind("gpx:", 0) == 0);
    ATF_REQUIRE_EQ(std::string(outings[0].source_type), "trail_running");
    ATF_REQUIRE_EQ(outings[0].avg_hr, 80);
    ATF_REQUIRE(outings[0].distance_m > 10.0);
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

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, parses_points_and_extensions);
    ATF_ADD_TEST_CASE(tcs, lists_and_loads_a_folder);
}
