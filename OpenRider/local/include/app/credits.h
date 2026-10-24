/// @file
/// The third-party components and data sources OpenRider builds on, as
/// Help > About lists them (module-level data). CREDITS.md holds the same
/// list with versions and licence texts; credits_test checks they agree.
#pragma once

namespace app {

// @impl 0f87-6adb-701e-a77c

/// One credited component or data source.
struct Credit {
    const char *name;     ///< as CREDITS.md links it
    const char *licence;  ///< short licence name
    const char *by;       ///< authors or provider, "" if none to name
};

/// Libraries and assets.
inline constexpr Credit kLibraryCredits[] = {
    {"Dear ImGui", "MIT", "Omar Cornut and contributors"},
    {"ImPlot", "MIT", "Evan Pezent"},
    {"GLFW", "zlib", "Marcus Geelnard, Camilla Loewy"},
    {"cJSON", "MIT", "Dave Gamble and contributors"},
    {"SQLite", "public domain", ""},
    {"stb_image", "public domain / MIT", "Sean Barrett"},
    {"libcurl", "curl licence", "Daniel Stenberg and contributors"},
    {"libtiff", "libtiff licence", "Sam Leffler, Silicon Graphics"},
    {"Phosphor icons", "MIT", "Phosphor Icons"},
};

/// Data sources: activities, maps, coordinate systems.
inline constexpr Credit kDataCredits[] = {
    {"GarminDB", "read only", "Tom Goetz"},
    {"OpenTopoMap", "CC-BY-SA", "map data OpenStreetMap contributors, SRTM"},
    {"IGN SCAN 25 Touristique", "IGN terms", "Geoplateforme"},
    {"Kartverket Topo", "CC BY 4.0", "Kartverket"},
    {"EPSG Geodetic Parameter Dataset", "EPSG terms of use", "IOGP"},
};

} // namespace app
