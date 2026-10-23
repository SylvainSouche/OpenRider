// Every projection method and datum shift against PROJ 9.7: cs2cs from
// WGS 84 to each shipped system's own PROJ string, at the centre of its
// area of use; and round trips.

#include <atf-c++.hpp>

#include "reproj/transformer.h"
#include "shipped.h"

#include <cmath>
#include <string>

namespace {

struct Reference {
    int epsg;
    double lon, lat;  // WGS84 degrees
    double x, y;      // PROJ's projected coordinates
};

const Reference kReferences[] = {
    {3857, 14.568400, 68.234600, 1621746.8697, 10517067.7830},  // WGS 84 / Pseudo-Mercator
    {3035, 4.625000, 54.665000, 3974432.4303, 3519463.6853},  // ETRS89-extended / LAEA Europe
    {3034, 4.625000, 54.665000, 3664622.8978, 3098904.2727},  // ETRS89-extended / LCC Europe
    {27700, -3.500000, 55.380000, 305055.4055, 610582.4482},  // OSGB36 / British National Grid
    {2157, -7.950000, 53.410000, 603324.3766, 739986.4341},  // IRENET95 / Irish Transverse Mercator
    {2154, 0.260000, 46.355000, 489353.5878, 6587552.2040},  // RGF93 v1 / Lambert-93
    {31370, 4.450000, 50.505000, 155763.8827, 132693.3827},  // BD72 / Belgian Lambert 72
    {3812, 4.450000, 50.505000, 655767.9696, 632694.7821},  // ETRS89 / Belgian Lambert 2008
    {28992, 5.210000, 52.225000, 142892.1926, 470783.8672},  // Amersfoort / RD New
    {2169, 6.130000, 49.815000, 77255.9940, 97842.8511},  // LUREF / Luxembourg TM
    {2056, 8.225000, 46.810000, 2660019.1119, 1184616.1678},  // CH1903+ / LV95
    {31287, 13.350000, 47.710000, 401306.8256, 423398.1835},  // MGI / Austria Lambert
    {3416, 13.350000, 47.710000, 401250.2908, 423340.5571},  // ETRS89 / Austria Lambert
    {6875, 12.460000, 40.930000, 7038685.0471, 4526102.1040},  // RDN2008 / Italy zone (N-E)
    {6707, 8.965000, 41.785000, 497091.6465, 4625906.0614},  // RDN2008 / UTM zone 32N (N-E)
    {6708, 15.000000, 40.945000, 500000.0000, 4532651.5693},  // RDN2008 / UTM zone 33N (N-E)
    {3763, -7.875000, 39.555000, 22183.2869, -12542.9338},  // ETRS89 / Portugal TM06
    {2180, 19.145000, 52.465000, 509847.9005, 511024.2427},  // ETRF2000-PL / CS92
    {5514, 17.325000, 49.395000, -544006.6689, -1144002.8118},  // S-JTSK / Krovak East North
    {23700, 19.505000, 47.160000, 684692.7847, 201865.8422},  // HD72 / EOV
    {3794, 14.995000, 46.150000, 499613.7701, 112247.5801},  // Slovenia 1996 / Slovene National Grid
    {3765, 16.430000, 44.440000, 494427.7816, 4922223.7584},  // HTRS96 / Croatia TM
    {3844, 25.835000, 45.855000, 564954.2243, 484257.9441},  // Pulkovo 1942(58) / Stereo70
    {7801, 25.520000, 42.735000, 501637.7009, 4733280.6853},  // BGS2005 / CCS2005
    {2100, 23.935000, 38.315000, 494168.0812, 4240480.1308},  // GGRS87 / Greek Grid
    {3006, 17.100000, 62.015000, 609924.4490, 6877630.3714},  // SWEREF99 TM
    {3067, 25.335000, 64.465000, 419924.3998, 7149882.0006},  // EUREF-FIN / TM35FIN(E,N)
    {8088, -18.210000, 64.775000, 2737577.9597, 275151.4256},  // ISN2016 / Lambert 2016
    {3301, 24.285000, 58.760000, 516493.1608, 6513417.9682},  // Estonian Coordinate System of 1997
    {3059, 23.650000, 56.880000, 478669.9097, 304082.7742},  // LKS-92 / Latvia TM
    {3346, 22.920000, 55.170000, 431194.0397, 6115464.6798},  // LKS94 / Lithuania TM
    {25830, -2.995000, 57.875000, 500296.5752, 6414793.6703},  // ETRS89 / UTM zone 30N
    {25833, 15.005000, 59.400000, 500283.9228, 6584593.6374},  // ETRS89 / UTM zone 33N
    {32633, 15.000000, 42.000000, 500000.0000, 4649776.2248},  // WGS 84 / UTM zone 33N
    {32734, 18.420000, -33.920000, 261488.8264, 6243716.3330},  // WGS 84 / UTM zone 34S
    // Far from the central meridian: Svolvær in Lambert-93, Brest 20
    // degrees off UTM 33N's central meridian.
    {2154, 14.5684, 68.2346, 1220996.1745, 9124926.0180},
    {25833, -4.4861, 48.3904, -938805.2199, 5546019.3825},
};

} // namespace

ATF_TEST_CASE_WITHOUT_HEAD(matches_proj);
ATF_TEST_CASE_BODY(matches_proj)
{
    const reproj::Crs wgs84 = ShippedCrs(4326);
    for (const Reference &r : kReferences) {
        const reproj::Transformer transformer(wgs84, ShippedCrs(r.epsg));
        double x, y;
        ATF_REQUIRE(transformer.Transform(r.lon, r.lat, x, y));
        // PROJ's values are rounded to 0.1 mm.
        if (std::fabs(x - r.x) > 0.001 || std::fabs(y - r.y) > 0.001)
            ATF_FAIL("EPSG:" + std::to_string(r.epsg) + ": off by " + std::to_string(x - r.x) + ", " +
                     std::to_string(y - r.y) + " m");
    }
}

ATF_TEST_CASE_WITHOUT_HEAD(round_trips);
ATF_TEST_CASE_BODY(round_trips)
{
    const reproj::Crs wgs84 = ShippedCrs(4326);
    for (int code : Shipped().Codes()) {
        // From the centre of the area of use and back: within a millimetre;
        // a few through a datum shift, whose height is dropped between the
        // two ellipsoids (as PROJ does).
        const reproj::Crs crs = ShippedCrs(code);
        const double lon = (crs.area.x0 + crs.area.x1) / 2.0, lat = (crs.area.y0 + crs.area.y1) / 2.0;
        const reproj::Transformer transformer(wgs84, crs);
        double x, y, back_lon, back_lat;
        ATF_REQUIRE(transformer.Transform(lon, lat, x, y));
        ATF_REQUIRE(transformer.Inverse().Transform(x, y, back_lon, back_lat));
        const double tolerance = crs.datum.shifted ? 3e-8 : 1e-8;
        if (std::fabs(back_lon - lon) > tolerance || std::fabs(back_lat - lat) > tolerance)
            ATF_FAIL(crs.Id() + ": round trip off by " + std::to_string(back_lon - lon) + ", " +
                     std::to_string(back_lat - lat) + " degrees");
    }
}

ATF_TEST_CASE_WITHOUT_HEAD(domains);
ATF_TEST_CASE_BODY(domains)
{
    double x, y;
    // Transverse Mercator: not beyond 60 degrees from the central meridian.
    ATF_REQUIRE(!ShippedCrs(25833).projection->Forward(-100.0, 45.0, x, y));
    // Mercator: not at the poles.
    ATF_REQUIRE(!ShippedCrs(3857).projection->Forward(0.0, 89.9, x, y));
    // Lambert-93: the south pole lies at infinity.
    ATF_REQUIRE(!ShippedCrs(2154).projection->Forward(0.0, -90.0, x, y));
    ATF_REQUIRE(ShippedCrs(2154).projection->Forward(0.0, 90.0, x, y));
    // LAEA Europe: not at the antipode of its centre.
    ATF_REQUIRE(!ShippedCrs(3035).projection->Forward(-170.0, -52.0, x, y));
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, matches_proj);
    ATF_ADD_TEST_CASE(tcs, round_trips);
    ATF_ADD_TEST_CASE(tcs, domains);
}
