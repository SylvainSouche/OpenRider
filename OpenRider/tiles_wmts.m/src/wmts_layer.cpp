#include "wmts_layer.h"

#include <cctype>
#include <utility>

namespace {

void ReplaceAll(std::string &text, const std::string &from, const std::string &to)
{
    for (size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size()))
        text.replace(at, from.size(), to);
}

// `text` percent-encoded for a query string value.
std::string Encoded(const std::string &text)
{
    static const char hex_digits[] = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : text) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == ':' || c == '/')
            out += static_cast<char>(c);
        else {
            out += '%';
            out += hex_digits[c >> 4];
            out += hex_digits[c & 15];
        }
    }
    return out;
}

} // namespace

std::string WmtsLayer::TileUrl(size_t grid, size_t level, int column, int row) const
{
    const WmtsGrid &g = grids[grid];
    const std::string &matrix = g.level_ids[level];
    if (!g.url_template.empty()) {
        std::string url = g.url_template;
        ReplaceAll(url, "{TileMatrixSet}", g.id);
        ReplaceAll(url, "{TileMatrix}", matrix);
        ReplaceAll(url, "{TileRow}", std::to_string(row));
        ReplaceAll(url, "{TileCol}", std::to_string(column));
        ReplaceAll(url, "{Style}", g.style);
        ReplaceAll(url, "{style}", g.style);
        return url;
    }
    // The endpoint may carry some parameters already (IGN's has
    // "SERVICE=WMTS&"): those are not repeated.
    std::string url = kvp_url;
    std::string present = url.find('?') == std::string::npos ? "" : url.substr(url.find('?'));
    for (char &c : present)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    url += url.find('?') == std::string::npos ? "?" : (url.back() == '?' || url.back() == '&' ? "" : "&");
    const std::pair<const char *, std::string> parameters[] = {
        {"SERVICE", "WMTS"},       {"REQUEST", "GetTile"},         {"VERSION", "1.0.0"},
        {"LAYER", Encoded(g.layer)}, {"STYLE", Encoded(g.style)},  {"TILEMATRIXSET", Encoded(g.id)},
        {"TILEMATRIX", Encoded(matrix)}, {"TILEROW", std::to_string(row)}, {"TILECOL", std::to_string(column)},
        {"FORMAT", Encoded(g.format)}};
    bool first = true;
    for (const auto &[name, value] : parameters) {
        const std::string key = std::string(name) + "=";
        if (present.find("?" + key) != std::string::npos || present.find("&" + key) != std::string::npos)
            continue;
        url += (first ? "" : "&") + key + value;
        first = false;
    }
    return url;
}
