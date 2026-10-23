#include "reproj/proj_string.h"

#include "reproj/geographic.h"
#include "reproj/krovak.h"
#include "reproj/lambert_azimuthal_equal_area.h"
#include "reproj/lambert_conformal_conic.h"
#include "reproj/mercator.h"
#include "reproj/oblique_stereographic.h"
#include "reproj/swiss_oblique_mercator.h"
#include "reproj/transverse_mercator.h"

#include <cmath>
#include <cstdlib>
#include <map>
#include <set>
#include <sstream>

namespace reproj {

namespace {

// Ellipsoids by PROJ name: semi-major axis and inverse flattening.
struct NamedEllipsoid {
    const char *name;
    double a, rf;
};
constexpr NamedEllipsoid kEllipsoids[] = {
    {"WGS84", 6378137.0, 298.257223563},  {"GRS80", 6378137.0, 298.257222101},
    {"GRS67", 6378160.0, 298.247167427},  {"airy", 6377563.396, 299.3249646},
    {"bessel", 6377397.155, 299.1528128}, {"intl", 6378388.0, 297.0},
    {"krass", 6378245.0, 298.3},          {"clrk66", 6378206.4, 294.9786982},
    {"clrk80ign", 6378249.2, 293.4660212936269},
};

// Parameters a method may take, beyond the ellipsoid and datum ones.
const std::map<std::string, std::set<std::string>> kMethodParameters = {
    {"longlat", {}},
    {"merc", {"lon_0", "lat_ts", "k", "k_0", "x_0", "y_0"}},
    {"webmerc", {"lon_0", "x_0", "y_0"}},
    {"tmerc", {"lat_0", "lon_0", "k", "k_0", "x_0", "y_0"}},
    {"utm", {"zone", "south"}},
    {"lcc", {"lat_0", "lon_0", "lat_1", "lat_2", "k", "k_0", "x_0", "y_0"}},
    {"sterea", {"lat_0", "lon_0", "k", "k_0", "x_0", "y_0"}},
    {"somerc", {"lat_0", "lon_0", "k", "k_0", "x_0", "y_0"}},
    {"krovak", {"lat_0", "lon_0", "alpha", "lat_ts", "k", "k_0", "x_0", "y_0"}},
    {"laea", {"lat_0", "lon_0", "x_0", "y_0"}},
};
// Parameters about the ellipsoid and datum, and noise PROJ itself adds.
const std::set<std::string> kCommonParameters = {"ellps", "datum", "a", "b", "rf", "f", "R", "towgs84",
                                                 "units", "no_defs", "type", "wktext", "nadgrids", "pm"};

} // namespace

std::shared_ptr<const Projection> ParseProjString(const std::string &definition, Datum &datum, bool &geographic,
                                                  std::string &error)
{
    // "+key=value" and "+flag" tokens.
    std::map<std::string, std::string> parameters;
    std::istringstream tokens(definition);
    std::string token;
    while (tokens >> token) {
        if (token[0] != '+' || token.size() < 2) {
            error = "not a PROJ parameter: " + token;
            return nullptr;
        }
        const size_t equals = token.find('=');
        parameters[token.substr(1, equals == std::string::npos ? std::string::npos : equals - 1)] =
            equals == std::string::npos ? "" : token.substr(equals + 1);
    }
    const std::string method = parameters.count("proj") ? parameters["proj"] : "";
    const std::string normalised = method == "latlong" || method == "lonlat" || method == "latlon" ? "longlat" : method;
    auto known = kMethodParameters.find(normalised);
    if (known == kMethodParameters.end()) {
        error = "unsupported projection method: " + (method.empty() ? std::string("(none)") : method);
        return nullptr;
    }
    for (const auto &[key, value] : parameters)
        if (key != "proj" && !known->second.count(key) && !kCommonParameters.count(key)) {
            error = "unsupported parameter for " + method + ": +" + key;
            return nullptr;
        }
    if (parameters.count("units") && parameters["units"] != "m") {
        error = "unsupported units: " + parameters["units"];
        return nullptr;
    }
    if (parameters.count("pm") && parameters["pm"] != "greenwich" && parameters["pm"] != "0") {
        error = "unsupported prime meridian: " + parameters["pm"];
        return nullptr;
    }
    if (parameters.count("nadgrids") && parameters["nadgrids"] != "@null") {
        error = "unsupported grid shift: " + parameters["nadgrids"];
        return nullptr;
    }
    auto number = [&](const char *key, double fallback) {
        auto found = parameters.find(key);
        return found == parameters.end() ? fallback : std::strtod(found->second.c_str(), nullptr);
    };

    // The ellipsoid: a datum, a named ellipsoid, or its axes.
    datum = Datum{};
    if (parameters.count("datum")) {
        if (parameters["datum"] != "WGS84") {
            error = "unsupported datum: " + parameters["datum"] + " (give +ellps and +towgs84)";
            return nullptr;
        }
    } else if (parameters.count("ellps")) {
        bool found = false;
        for (const NamedEllipsoid &named : kEllipsoids)
            if (parameters["ellps"] == named.name) {
                datum.ellipsoid = {named.a, 1.0 / named.rf};
                found = true;
            }
        if (!found) {
            error = "unknown ellipsoid: " + parameters["ellps"];
            return nullptr;
        }
    } else if (parameters.count("a") || parameters.count("R")) {
        const double a = number(parameters.count("R") ? "R" : "a", 0.0);
        double f = 0.0;
        if (parameters.count("b"))
            f = 1.0 - number("b", a) / a;
        else if (parameters.count("rf"))
            f = 1.0 / number("rf", 0.0);
        else if (parameters.count("f"))
            f = number("f", 0.0);
        datum.ellipsoid = {a, f};
    }
    if (!(datum.ellipsoid.a > 0.0) || !(datum.ellipsoid.f >= 0.0 && datum.ellipsoid.f < 1.0)) {
        error = "invalid ellipsoid";
        return nullptr;
    }
    if (parameters.count("towgs84")) {
        std::istringstream values(parameters["towgs84"]);
        std::string value;
        int count = 0;
        while (std::getline(values, value, ',') && count < 7)
            datum.towgs84[count++] = std::strtod(value.c_str(), nullptr);
        if (count != 3 && count != 7) {
            error = "+towgs84 needs 3 or 7 values";
            return nullptr;
        }
        for (double v : datum.towgs84)
            datum.shifted = datum.shifted || v != 0.0;
    }

    // The projection.
    const Ellipsoid &ellipsoid = datum.ellipsoid;
    const double lat0 = number("lat_0", 0.0), lon0 = number("lon_0", 0.0);
    const double x0 = number("x_0", 0.0), y0 = number("y_0", 0.0);
    const double k0 = number("k", number("k_0", 1.0));
    geographic = normalised == "longlat";
    if (geographic)
        return std::make_shared<Geographic>();
    if (normalised == "webmerc")
        return std::make_shared<Mercator>(Ellipsoid{ellipsoid.a, 0.0}, lon0, 1.0, x0, y0);
    if (normalised == "merc") {
        // A true-scale latitude sets the equator's scale instead of k.
        double scale = k0;
        if (parameters.count("lat_ts")) {
            const double e2 = ellipsoid.f * (2.0 - ellipsoid.f), phi = number("lat_ts", 0.0) * 3.14159265358979323846 / 180.0;
            scale = std::cos(phi) / std::sqrt(1.0 - e2 * std::sin(phi) * std::sin(phi));
        }
        return std::make_shared<Mercator>(ellipsoid, lon0, scale, x0, y0);
    }
    if (normalised == "tmerc")
        return std::make_shared<TransverseMercator>(ellipsoid, lat0, lon0, k0, x0, y0);
    if (normalised == "utm") {
        const int zone = static_cast<int>(number("zone", 0.0));
        if (zone < 1 || zone > 60) {
            error = "+zone must be 1..60";
            return nullptr;
        }
        return std::make_shared<TransverseMercator>(ellipsoid, 0.0, zone * 6.0 - 183.0, 0.9996, 500000.0,
                                                    parameters.count("south") ? 10000000.0 : 0.0);
    }
    if (normalised == "lcc") {
        if (!parameters.count("lat_1")) {
            error = "lcc needs +lat_1";
            return nullptr;
        }
        const double lat1 = number("lat_1", 0.0);
        return std::make_shared<LambertConformalConic>(ellipsoid, lat0, lon0, lat1, number("lat_2", lat1), k0, x0, y0);
    }
    if (normalised == "sterea")
        return std::make_shared<ObliqueStereographic>(ellipsoid, lat0, lon0, k0, x0, y0);
    if (normalised == "somerc")
        return std::make_shared<SwissObliqueMercator>(ellipsoid, lat0, lon0, k0, x0, y0);
    if (normalised == "krovak")
        return std::make_shared<Krovak>(ellipsoid, number("lat_0", 49.5), number("lon_0", 24.8333333333333),
                                        number("alpha", 30.2881397527778), number("lat_ts", 78.5),
                                        number("k", number("k_0", 0.9999)), x0, y0);
    return std::make_shared<LambertAzimuthalEqualArea>(ellipsoid, lat0, lon0, x0, y0);
}

} // namespace reproj
