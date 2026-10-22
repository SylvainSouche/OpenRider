#include "wmts_capabilities.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <map>
#include <set>
#include <sstream>

namespace {

// Metres per degree on the WGS 84 equator, for geographic grids
// (OGC 07-057r7, 6.1).
constexpr double kMetresPerDegree = 2.0 * 3.14159265358979323846 * 6378137.0 / 360.0;
// Size of a "standardized rendering pixel", metres.
constexpr double kPixelSize = 0.00028;

// Geographic systems, in degrees.
const std::set<int> kGeographic = {4326, 4258, 4171, 4269};
// Systems whose EPSG axis order puts the northing (or latitude) first, as
// the TopLeftCorner then does (from PROJ's database for the systems
// OpenRider ships).
const std::set<int> kNorthingFirst = {4326, 4258, 4171, 4269, 3035, 3034, 2169, 31287, 3416, 6875,
                                      6707, 6708, 2180, 3844, 7801, 3006, 3301, 3059, 3346};
// Systems known by another authority's name.
const std::map<std::string, std::string> kAliases = {
    {"IGNF:LAMB93", "EPSG:2154"}, {"IGNF:RGF93LAMB93", "EPSG:2154"}, {"CRS84", "EPSG:4326"}};

// EPSG code of "EPSG:<code>", or 0.
int CodeOf(const std::string &crs)
{
    return crs.compare(0, 5, "EPSG:") == 0 ? std::atoi(crs.c_str() + 5) : 0;
}

// The numbers of a whitespace-separated list.
std::vector<double> Numbers(const std::string &text)
{
    std::vector<double> numbers;
    std::istringstream stream(text);
    double value;
    while (stream >> value)
        numbers.push_back(value);
    return numbers;
}

// The node's Identifier child (ows:Identifier).
std::string IdentifierOf(const XmlNode &node)
{
    return node.ChildText("Identifier");
}

} // namespace

std::string NormalisedCrs(const std::string &supported_crs)
{
    std::string crs = supported_crs;
    // CRS84 is longitude-latitude WGS 84, whatever its URN.
    if (crs.find("CRS84") != std::string::npos)
        return "CRS84";
    if (auto alias = kAliases.find(crs); alias != kAliases.end())
        return alias->second;
    if (crs.find("EPSG") == std::string::npos)
        return crs;
    // The code is the last run of digits: "urn:ogc:def:crs:EPSG:6.18:3:3857",
    // "http://www.opengis.net/def/crs/EPSG/0/2154", "EPSG:25833".
    size_t end = crs.size();
    while (end > 0 && !std::isdigit(static_cast<unsigned char>(crs[end - 1])))
        --end;
    size_t start = end;
    while (start > 0 && std::isdigit(static_cast<unsigned char>(crs[start - 1])))
        --start;
    return start < end ? "EPSG:" + crs.substr(start, end - start) : crs;
}

std::string ReadGrids(const XmlNode &capabilities, const std::vector<std::string> &layers,
                      std::vector<WmtsGrid> &grids, std::string &kvp_url)
{
    grids.clear();
    kvp_url.clear();
    // The GetTile KVP endpoint, if the service declares one.
    if (const XmlNode *operations = capabilities.Child("OperationsMetadata"))
        for (const XmlNode *operation : operations->Children("Operation"))
            if (operation->Attribute("name") == "GetTile")
                if (const XmlNode *dcp = operation->Child("DCP"))
                    if (const XmlNode *http = dcp->Child("HTTP"))
                        for (const XmlNode *get : http->Children("Get")) {
                            // A Get restricted to REST encoding is not the KVP one.
                            const std::string encoding = get->Child("Constraint") ? get->Child("Constraint")->ChildText("AllowedValues") : "";
                            if (encoding.find("RESTful") == std::string::npos || encoding.find("KVP") != std::string::npos)
                                kvp_url = get->Attribute("href");
                        }
    const XmlNode *contents = capabilities.Child("Contents");
    if (!contents)
        return "no Contents in the capabilities";
    std::map<std::string, const XmlNode *> sets;
    for (const XmlNode *set : contents->Children("TileMatrixSet"))
        sets[IdentifierOf(*set)] = set;
    std::set<std::string> systems;
    for (const std::string &wanted : layers) {
        const XmlNode *layer = nullptr;
        for (const XmlNode *candidate : contents->Children("Layer"))
            if (IdentifierOf(*candidate) == wanted)
                layer = candidate;
        if (!layer)
            return "no layer " + wanted + " in the capabilities";
        // Default style, preferred format, REST template.
        std::string style;
        for (const XmlNode *candidate : layer->Children("Style"))
            if (style.empty() || candidate->Attribute("isDefault") == "true")
                style = IdentifierOf(*candidate);
        std::string format;
        for (const XmlNode *candidate : layer->Children("Format"))
            if (format.empty() || candidate->text == "image/png")
                format = candidate->text;
        std::string url_template;
        for (const XmlNode *resource : layer->Children("ResourceURL"))
            if (resource->Attribute("resourceType") == "tile" && (url_template.empty() || resource->Attribute("format") == format))
                url_template = resource->Attribute("template");
        if (url_template.empty() && kvp_url.empty())
            return "layer " + wanted + " has neither a ResourceURL nor a KVP GetTile endpoint";
        for (const XmlNode *link : layer->Children("TileMatrixSetLink")) {
            const std::string set_id = link->ChildText("TileMatrixSet");
            auto set = sets.find(set_id);
            if (set == sets.end())
                continue;
            const std::string crs = NormalisedCrs(set->second->ChildText("SupportedCRS"));
            if (!systems.insert(crs).second)
                continue;  // a grid in this system already
            // The layer's limits per tile matrix, when given.
            std::map<std::string, const XmlNode *> limits;
            if (const XmlNode *all = link->Child("TileMatrixSetLimits"))
                for (const XmlNode *limit : all->Children("TileMatrixLimits"))
                    limits[limit->ChildText("TileMatrix")] = limit;
            const int code = CodeOf(crs);
            const bool geographic = kGeographic.count(code) || crs == "CRS84";
            const bool northing_first = kNorthingFirst.count(code) && crs != "CRS84";
            WmtsGrid grid;
            grid.id = set_id;
            grid.crs = crs == "CRS84" ? "EPSG:4326" : crs;
            grid.layer = wanted;
            grid.style = style;
            grid.format = format;
            grid.url_template = url_template;
            for (const XmlNode *matrix : set->second->Children("TileMatrix")) {
                const std::string id = IdentifierOf(*matrix);
                const XmlNode *limit = limits.empty() ? nullptr : (limits.count(id) ? limits[id] : nullptr);
                if (!limits.empty() && !limit)
                    continue;  // not served for this layer
                const std::vector<double> corner = Numbers(matrix->ChildText("TopLeftCorner"));
                const double scale = std::atof(matrix->ChildText("ScaleDenominator").c_str());
                if (corner.size() < 2 || !(scale > 0.0))
                    continue;
                or_tile_matrix level{};
                level.resolution = scale * kPixelSize / (geographic ? kMetresPerDegree : 1.0);
                level.origin_x = northing_first ? corner[1] : corner[0];
                level.origin_y = northing_first ? corner[0] : corner[1];
                level.tile_width = std::atoi(matrix->ChildText("TileWidth").c_str());
                level.tile_height = std::atoi(matrix->ChildText("TileHeight").c_str());
                level.matrix_width = std::atoi(matrix->ChildText("MatrixWidth").c_str());
                level.matrix_height = std::atoi(matrix->ChildText("MatrixHeight").c_str());
                level.first_column = limit ? std::atoi(limit->ChildText("MinTileCol").c_str()) : 0;
                level.last_column = limit ? std::atoi(limit->ChildText("MaxTileCol").c_str()) : level.matrix_width - 1;
                level.first_row = limit ? std::atoi(limit->ChildText("MinTileRow").c_str()) : 0;
                level.last_row = limit ? std::atoi(limit->ChildText("MaxTileRow").c_str()) : level.matrix_height - 1;
                if (level.tile_width <= 0 || level.tile_height <= 0 || level.matrix_width <= 0 || level.matrix_height <= 0)
                    continue;
                grid.level_ids.push_back(id);
                grid.levels.push_back(level);
            }
            if (grid.levels.empty())
                continue;
            // Coarsest first; ids are attached once the vectors stop moving.
            std::vector<size_t> order(grid.levels.size());
            for (size_t k = 0; k < order.size(); ++k)
                order[k] = k;
            std::sort(order.begin(), order.end(),
                      [&](size_t a, size_t b) { return grid.levels[a].resolution > grid.levels[b].resolution; });
            WmtsGrid sorted = grid;
            for (size_t k = 0; k < order.size(); ++k) {
                sorted.levels[k] = grid.levels[order[k]];
                sorted.level_ids[k] = grid.level_ids[order[k]];
            }
            grids.push_back(std::move(sorted));
        }
    }
    for (WmtsGrid &grid : grids)
        for (size_t k = 0; k < grid.levels.size(); ++k)
            grid.levels[k].id = grid.level_ids[k].c_str();
    return grids.empty() ? "no usable tile matrix set" : "";
}
