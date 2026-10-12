#include "map.h"

#include <imgui.h>
#include <GLFW/glfw3.h>

#include "core/paths.h"
#include "core/plugins.h"
#include "maptiles/coverage.h"
#include "maptiles/mercator.h"
#include "maptiles/screen.h"

#include <dirent.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace {

constexpr size_t kMaxTextures = 400;
constexpr int kMaxViewZoom = 20;  // beyond the layer's last level, tiles are enlarged
constexpr double kRetrySeconds = 30.0;

double now_s() { return glfwGetTime(); }

unsigned int upload(const maptiles::Image &img)
{
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img.width, img.height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 img.rgba.data());
    return tex;
}

void collect_ready(MapView &m)
{
    for (maptiles::ReadyTile &r : m.fetcher->take_ready()) {
        if (r.image.rgba.empty()) {
            m.retry_after[r.key] = now_s() + kRetrySeconds;
            continue;
        }
        MapTexture &t = m.textures[r.key];
        if (t.gl)
            glDeleteTextures(1, &t.gl);
        t.gl = upload(r.image);
        t.last_used = m.frame;
    }
    // Least recently drawn textures go first when there are too many.
    while (m.textures.size() > kMaxTextures) {
        auto oldest = m.textures.begin();
        for (auto it = m.textures.begin(); it != m.textures.end(); ++it)
            if (it->second.last_used < oldest->second.last_used)
                oldest = it;
        if (oldest->second.gl)
            glDeleteTextures(1, &oldest->second.gl);
        m.textures.erase(oldest);
    }
}

void fit_to_track(MapView &m, const std::vector<or_point> &pts, float w, float h)
{
    double west = 180, east = -180, south = 90, north = -90;
    for (const or_point &p : pts) {
        if (std::isnan(p.lat) || std::isnan(p.lon))
            continue;
        west = std::min(west, p.lon);
        east = std::max(east, p.lon);
        south = std::min(south, p.lat);
        north = std::max(north, p.lat);
    }
    if (west > east)
        return;
    const int ts = m.info->tile_size;
    m.zoom = maptiles::fit_zoom(west, south, east, north, w * 0.9, h * 0.9, ts, m.info->min_zoom,
                                std::max(m.info->max_zoom, kMaxViewZoom - 2));
    double x0, y0, x1, y1;
    maptiles::lonlat_to_world(west, north, m.zoom, ts, x0, y0);
    maptiles::lonlat_to_world(east, south, m.zoom, ts, x1, y1);
    m.cx = (x0 + x1) / 2.0;
    m.cy = (y0 + y1) / 2.0;
}

void zoom_at(MapView &m, int dz, double px, double py)
{
    const int z = std::clamp(m.zoom + dz, m.info->min_zoom, std::max(kMaxViewZoom, m.info->max_zoom));
    if (z == m.zoom)
        return;
    // Keep the world point under the cursor fixed.
    const double k = std::pow(2.0, z - m.zoom);
    const double wx = m.cx + px, wy = m.cy + py;
    m.cx = wx * k - px;
    m.cy = wy * k - py;
    m.zoom = z;
}

// Blue -> cyan -> green -> yellow -> red.
ImU32 gradient(double t)
{
    static const float stops[5][3] = {
        {0.10f, 0.30f, 1.00f}, {0.00f, 0.80f, 0.90f}, {0.10f, 0.75f, 0.10f},
        {0.95f, 0.85f, 0.05f}, {0.90f, 0.10f, 0.10f}};
    t = std::clamp(t, 0.0, 1.0) * 4.0;
    const int i = std::min(3, static_cast<int>(t));
    const float f = static_cast<float>(t - i);
    float c[3];
    for (int k = 0; k < 3; ++k)
        c[k] = stops[i][k] + (stops[i + 1][k] - stops[i][k]) * f;
    return ImGui::ColorConvertFloat4ToU32(ImVec4(c[0], c[1], c[2], 0.95f));
}

// 5th and 95th percentiles of the defined values, so a few outliers do not
// flatten the gradient.
bool value_range(const std::vector<double> &v, double &lo, double &hi)
{
    std::vector<double> d;
    for (double x : v)
        if (!std::isnan(x))
            d.push_back(x);
    if (d.size() < 2)
        return false;
    std::sort(d.begin(), d.end());
    lo = d[d.size() * 5 / 100];
    hi = d[std::min(d.size() - 1, d.size() * 95 / 100)];
    if (hi <= lo)
        hi = lo + 1.0;
    return true;
}

} // namespace

namespace {

// Opens `json` with the first tiles plugin accepting it.
std::string open_layer(MapView &m, const std::string &json)
{
    const core::PluginScan scan =
        core::load_plugins({core::bundled_plugins_dir(), core::user_plugins_dir()}, OR_PLUGIN_TILES);
    std::string why = scan.loaded.empty() ? "no tiles plugin found" : "";
    for (const core::LoadedPlugin &p : scan.loaded) {
        const or_tiles_vtable *vt = static_cast<const or_tiles_vtable *>(p.info->vtable);
        char err[256] = "";
        void *layer = vt->open(json.c_str(), err, sizeof err);
        if (!layer) {
            why += std::string(p.info->name) + ": " + err + "; ";
            continue;
        }
        m.vt = vt;
        m.layer = layer;
        m.info = vt->layer(layer);
        m.cache = std::make_unique<maptiles::TileCache>(core::user_cache_dir() + "/tiles",
                                                        maptiles::kDefaultCacheBytes);
        m.fetcher = std::make_unique<maptiles::TileFetcher>(
            vt, layer, m.cache.get(), "OpenRider/0.1 (desktop outing viewer; libcurl)");
        return "";
    }
    return why;
}

std::vector<std::string> json_files(const std::string &dir)
{
    std::vector<std::string> out;
    if (DIR *d = opendir(dir.c_str())) {
        while (dirent *e = readdir(d)) {
            const std::string n = e->d_name;
            if (n.size() > 5 && n[0] != '.' && n.compare(n.size() - 5, 5, ".json") == 0)
                out.push_back(dir + "/" + n);
        }
        closedir(d);
    }
    std::sort(out.begin(), out.end());
    return out;
}

std::string read_text(const std::string &path)
{
    std::string out;
    if (FILE *f = std::fopen(path.c_str(), "rb")) {
        char buf[4096];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof buf, f)) > 0)
            out.append(buf, n);
        std::fclose(f);
    }
    return out;
}

} // namespace

std::vector<std::string> map_load_layers(MapView &m, const std::vector<std::string> &dirs)
{
    std::vector<std::string> problems;
    const core::PluginScan scan =
        core::load_plugins({core::bundled_plugins_dir(), core::user_plugins_dir()}, OR_PLUGIN_TILES);
    for (const std::string &dir : dirs) {
        for (const std::string &path : json_files(dir)) {
            MapLayerChoice c;
            c.json = read_text(path);
            bool ok = false;
            for (const core::LoadedPlugin &p : scan.loaded) {
                const or_tiles_vtable *vt = static_cast<const or_tiles_vtable *>(p.info->vtable);
                char err[256] = "";
                void *h = vt->open(c.json.c_str(), err, sizeof err);
                if (!h)
                    continue;
                const or_tile_layer *l = vt->layer(h);
                c.id = l->id;
                c.name = l->name;
                for (int k = 0; k < 4; ++k)
                    c.extent[k] = l->extent[k];
                vt->close(h);
                ok = true;
                break;
            }
            if (ok)
                m.layers.push_back(c);
            else
                problems.push_back("map layer " + path + " is not usable by any tiles plugin");
        }
    }
    return problems;
}

// @impl 0f87-6acd-3542-dba2
std::string map_select(MapView &m, int index)
{
    if (index < 0 || index >= static_cast<int>(m.layers.size()) || index == m.current)
        return "";
    // Keep the view centre across layers.
    double lon = 6.63, lat = 45.94;  // Alps until a track is shown
    if (m.info)
        maptiles::world_to_lonlat(m.cx, m.cy, m.zoom, m.info->tile_size, lon, lat);
    map_close(m);
    const std::string err = open_layer(m, m.layers[static_cast<size_t>(index)].json);
    if (!err.empty())
        return err;
    m.current = index;
    m.zoom = std::clamp(m.zoom, m.info->min_zoom, std::max(kMaxViewZoom, m.info->max_zoom));
    maptiles::lonlat_to_world(lon, lat, m.zoom, m.info->tile_size, m.cx, m.cy);
    m.retry_after.clear();
    m.unavailable_reported = false;
    return "";
}

std::vector<int> map_offered_layers(const MapView &m, const std::vector<or_point> &points,
                                    float width, float height)
{
    std::vector<int> out;
    if (!m.info)
        return out;
    maptiles::GeoBox view{};
    maptiles::world_to_lonlat(m.cx - width / 2.0, m.cy + height / 2.0, m.zoom, m.info->tile_size,
                              view.west, view.south);
    maptiles::world_to_lonlat(m.cx + width / 2.0, m.cy - height / 2.0, m.zoom, m.info->tile_size,
                              view.east, view.north);
    std::vector<double> lons, lats;
    for (const or_point &p : points) {
        lons.push_back(p.lon);
        lats.push_back(p.lat);
    }
    for (int i = 0; i < static_cast<int>(m.layers.size()); ++i)
        if (i == m.current || maptiles::layer_offered(m.layers[static_cast<size_t>(i)].extent, view, lons, lats))
            out.push_back(i);
    return out;
}

void map_close(MapView &m)
{
    m.fetcher.reset();  // joins the workers before the layer goes away
    m.cache.reset();
    for (auto &t : m.textures)
        if (t.second.gl)
            glDeleteTextures(1, &t.second.gl);
    m.textures.clear();
    if (m.vt && m.layer)
        m.vt->close(m.layer);
    m.layer = nullptr;
    m.info = nullptr;
    m.vt = nullptr;
    m.current = -1;
}

namespace {

// What a map drawing shares: the region, its world-pixel offset, the input.
struct MapFrame {
    ImVec2 origin, end;
    double left = 0.0, top = 0.0;
    ImDrawList *dl = nullptr;
    bool clicked = false;  // released without dragging
    ImVec2 mouse;
};

// Input (drag to pan, wheel to zoom) and the background tiles.
void begin_map(MapView &m, float width, float height, MapFrame &f)
{
    ++m.frame;
    collect_ready(m);
    f.origin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##map", ImVec2(width, height));
    const bool hovered = ImGui::IsItemHovered();
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
        const ImVec2 d = ImGui::GetIO().MouseDelta;
        m.cx -= d.x;
        m.cy -= d.y;
    }
    f.mouse = ImGui::GetIO().MousePos;
    f.clicked = ImGui::IsItemDeactivated() &&
                ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f).x == 0.0f &&
                ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f).y == 0.0f;
    if (hovered && ImGui::GetIO().MouseWheel != 0.0f)
        zoom_at(m, ImGui::GetIO().MouseWheel > 0.0f ? 1 : -1, f.mouse.x - f.origin.x - width / 2.0,
                f.mouse.y - f.origin.y - height / 2.0);

    f.dl = ImGui::GetWindowDrawList();
    f.end = ImVec2(f.origin.x + width, f.origin.y + height);
    f.dl->PushClipRect(f.origin, f.end, true);
    f.dl->AddRectFilled(f.origin, f.end, IM_COL32(200, 202, 205, 255));

    // Tiles come from level tz = min(view zoom, layer max); past the
    // layer's last level each tile is drawn enlarged (over-zoom).
    // @impl 0f87-6acd-5daf-1652
    const int tz = std::min(m.zoom, m.info->max_zoom);
    const double scale = std::pow(2.0, m.zoom - tz);
    const double ts = m.info->tile_size * scale;  // on-screen tile size
    const int n = 1 << tz;
    f.left = m.cx - width / 2.0;
    f.top = m.cy - height / 2.0;
    const int tx0 = static_cast<int>(std::floor(f.left / ts));
    const int tx1 = static_cast<int>(std::floor((f.left + width) / ts));
    const int ty0 = std::max(0, static_cast<int>(std::floor(f.top / ts)));
    const int ty1 = std::min(n - 1, static_cast<int>(std::floor((f.top + height) / ts)));
    const double now = now_s();
    for (int ty = ty0; ty <= ty1; ++ty) {
        for (int tx = tx0; tx <= tx1; ++tx) {
            const maptiles::TileKey key{tz, ((tx % n) + n) % n, ty};
            const ImVec2 p0(static_cast<float>(f.origin.x + tx * ts - f.left),
                            static_cast<float>(f.origin.y + ty * ts - f.top));
            const ImVec2 p1(static_cast<float>(p0.x + ts), static_cast<float>(p0.y + ts));
            auto it = m.textures.find(key);
            if (it != m.textures.end() && it->second.gl) {
                it->second.last_used = m.frame;
                f.dl->AddImage(static_cast<ImTextureID>(it->second.gl), p0, p1);
                continue;
            }
            auto retry = m.retry_after.find(key);
            if (retry == m.retry_after.end() || retry->second <= now)
                m.fetcher->request(key);
        }
    }
}

// Attribution, end of clipping; a message when the layer just became
// unavailable.
std::string end_map(MapView &m, MapFrame &f)
{
    ImDrawList *dl = f.dl;
    const ImVec2 end = f.end;
    // Attribution, always visible while the layer is shown.
    if (m.info->attribution && *m.info->attribution) {
        const ImVec2 sz = ImGui::CalcTextSize(m.info->attribution);
        const ImVec2 a(end.x - sz.x - 6.0f, end.y - sz.y - 4.0f);
        dl->AddRectFilled(ImVec2(a.x - 4.0f, a.y - 2.0f), end, IM_COL32(255, 255, 255, 200));
        dl->AddText(a, IM_COL32(40, 40, 40, 255), m.info->attribution);
    }
    dl->PopClipRect();

    if (m.fetcher->unavailable() && !m.unavailable_reported) {
        m.unavailable_reported = true;
        return std::string("Map layer ") + m.info->name + " is unavailable: none of its servers respond.";
    }
    if (!m.fetcher->unavailable())
        m.unavailable_reported = false;
    return "";
}

} // namespace

// @impl 0f87-6acd-2f33-0934
std::string map_draw(MapView &m, const std::vector<or_point> &points, const std::string &outing_id,
                     float width, float height, const TrackPaint &paint)
{
    if (!m.fetcher) {
        ImGui::TextDisabled("No map layer");
        return "";
    }
    if (outing_id != m.fitted_id && !points.empty()) {
        fit_to_track(m, points, width, height);
        m.fitted_id = outing_id;
    }
    MapFrame f;
    begin_map(m, width, height, f);
    ImDrawList *dl = f.dl;
    const ImVec2 origin = f.origin;
    const double left = f.left, top = f.top;
    const int ts = m.info->tile_size;

    // The track, in world pixels at the current zoom: one colour, or
    // coloured per segment by the chosen value.
    // @impl 0f87-6acd-32ed-0d05
    std::vector<ImVec2> line(points.size());
    std::vector<bool> valid(points.size(), false);
    for (size_t i = 0; i < points.size(); ++i) {
        const or_point &p = points[i];
        if (std::isnan(p.lat) || std::isnan(p.lon))
            continue;
        double x, y;
        maptiles::lonlat_to_world(p.lon, p.lat, m.zoom, ts, x, y);
        line[i] = ImVec2(static_cast<float>(origin.x + x - left), static_cast<float>(origin.y + y - top));
        valid[i] = true;
    }
    double lo = 0.0, hi = 1.0;
    const bool coloured = paint.values && paint.values->size() == points.size() &&
                          value_range(*paint.values, lo, hi);
    // Drop points closer than 1.5 px to the last drawn one: sub-pixel GPS
    // zigzags would otherwise become miter spikes on a thick line.
    std::vector<size_t> idx;
    std::vector<float> xs, ys;
    for (size_t i = 0; i < points.size(); ++i)
        if (valid[i]) {
            idx.push_back(i);
            xs.push_back(line[i].x);
            ys.push_back(line[i].y);
        }
    std::vector<size_t> drawn;
    for (size_t k : maptiles::simplify_screen(xs, ys, maptiles::kMinSegmentPx))
        drawn.push_back(idx[k]);
    if (!coloured) {
        std::vector<ImVec2> run;
        for (size_t i : drawn)
            run.push_back(line[i]);
        if (run.size() > 1)
            dl->AddPolyline(run.data(), static_cast<int>(run.size()), paint.solid, ImDrawFlags_None, 3.0f);
    } else {
        size_t prev = points.size();
        for (size_t i : drawn) {
            if (prev < points.size()) {
                const double v = (*paint.values)[i];
                const ImU32 col = std::isnan(v) ? IM_COL32(120, 120, 120, 220) : gradient((v - lo) / (hi - lo));
                dl->AddLine(line[prev], line[i], col, 3.5f);
            }
            prev = i;
        }
        // Legend: gradient bar with the range it spans.
        const ImVec2 l0(origin.x + 8.0f, origin.y + 8.0f);
        const float bw = 120.0f, bh = 8.0f;
        dl->AddRectFilled(ImVec2(l0.x - 4, l0.y - 4), ImVec2(l0.x + bw + 4, l0.y + bh + 34),
                          IM_COL32(255, 255, 255, 210), 3.0f);
        for (int k = 0; k < 24; ++k)
            dl->AddRectFilled(ImVec2(l0.x + bw * k / 24.0f, l0.y + 14), ImVec2(l0.x + bw * (k + 1) / 24.0f, l0.y + 14 + bh),
                              gradient(k / 23.0));
        dl->AddText(l0, IM_COL32(40, 40, 40, 255), paint.label);
        char buf[32];
        std::snprintf(buf, sizeof buf, "%.0f", lo);
        dl->AddText(ImVec2(l0.x, l0.y + 16 + bh), IM_COL32(40, 40, 40, 255), buf);
        std::snprintf(buf, sizeof buf, "%.0f", hi);
        dl->AddText(ImVec2(l0.x + bw - ImGui::CalcTextSize(buf).x, l0.y + 16 + bh), IM_COL32(40, 40, 40, 255), buf);
    }
    // The point hovered in the graphs.
    if (paint.hover_index >= 0 && static_cast<size_t>(paint.hover_index) < points.size() &&
        valid[static_cast<size_t>(paint.hover_index)]) {
        const ImVec2 c = line[static_cast<size_t>(paint.hover_index)];
        dl->AddCircleFilled(c, 7.0f, IM_COL32(255, 255, 255, 255));
        dl->AddCircleFilled(c, 5.0f, IM_COL32(20, 20, 20, 255));
    }

    return end_map(m, f);
}

maptiles::GeoBox map_view_box(const MapView &m, float width, float height)
{
    maptiles::GeoBox b{-180, -85, 180, 85};
    if (!m.info)
        return b;
    maptiles::world_to_lonlat(m.cx - width / 2.0, m.cy + height / 2.0, m.zoom, m.info->tile_size,
                              b.west, b.south);
    maptiles::world_to_lonlat(m.cx + width / 2.0, m.cy - height / 2.0, m.zoom, m.info->tile_size,
                              b.east, b.north);
    return b;
}

MapViewState map_get_state(const MapView &m)
{
    MapViewState st;
    if (!m.info)
        return st;
    maptiles::world_to_lonlat(m.cx, m.cy, m.zoom, m.info->tile_size, st.lon, st.lat);
    st.zoom = m.zoom;
    st.valid = true;
    return st;
}

void map_set_state(MapView &m, const MapViewState &st)
{
    if (!m.info || !st.valid)
        return;
    m.zoom = std::clamp(st.zoom, m.info->min_zoom, std::max(kMaxViewZoom, m.info->max_zoom));
    maptiles::lonlat_to_world(st.lon, st.lat, m.zoom, m.info->tile_size, m.cx, m.cy);
}

// @impl 0f87-6acd-5b43-c60a
std::string map_draw_overview(MapView &m, const std::vector<const core::OverviewTrack *> &tracks,
                              const std::vector<unsigned int> &colours,
                              float width, float height, bool fit, std::string &hovered_id,
                              std::string &message)
{
    hovered_id.clear();
    if (!m.fetcher) {
        ImGui::TextDisabled("No map layer");
        return "";
    }
    if (fit && !tracks.empty()) {
        double w = 180, e = -180, so = 90, no = -90;
        for (const core::OverviewTrack *tp : tracks) {
            const core::OverviewTrack &t = *tp;
            w = std::min(w, t.west);
            e = std::max(e, t.east);
            so = std::min(so, t.south);
            no = std::max(no, t.north);
        }
        const int ts = m.info->tile_size;
        m.zoom = maptiles::fit_zoom(w, so, e, no, width * 0.9, height * 0.9, ts, m.info->min_zoom,
                                    std::max(m.info->max_zoom, kMaxViewZoom - 2));
        double x0, y0, x1, y1;
        maptiles::lonlat_to_world(w, no, m.zoom, ts, x0, y0);
        maptiles::lonlat_to_world(e, so, m.zoom, ts, x1, y1);
        m.cx = (x0 + x1) / 2.0;
        m.cy = (y0 + y1) / 2.0;
    }
    MapFrame f;
    begin_map(m, width, height, f);
    const maptiles::GeoBox view = map_view_box(m, width, height);
    const int ts = m.info->tile_size;

    // Screen positions of the visible tracks; the one nearest the mouse
    // (within 6 px) is the hovered one.
    std::vector<std::vector<ImVec2>> lines(tracks.size());
    const bool mouse_in = f.mouse.x >= f.origin.x && f.mouse.x < f.end.x && f.mouse.y >= f.origin.y &&
                          f.mouse.y < f.end.y && ImGui::IsItemHovered();
    float best = 6.0f * 6.0f;
    int best_track = -1;
    for (size_t k = 0; k < tracks.size(); ++k) {
        const core::OverviewTrack &t = *tracks[k];
        if (t.east < view.west || t.west > view.east || t.north < view.south || t.south > view.north)
            continue;
        std::vector<ImVec2> &line = lines[k];
        line.reserve(t.lat.size());
        for (size_t i = 0; i < t.lat.size(); ++i) {
            double x, y;
            maptiles::lonlat_to_world(t.lon[i], t.lat[i], m.zoom, ts, x, y);
            const ImVec2 p(static_cast<float>(f.origin.x + x - f.left), static_cast<float>(f.origin.y + y - f.top));
            line.push_back(p);
            if (mouse_in) {
                const float dx = p.x - f.mouse.x, dy = p.y - f.mouse.y;
                if (dx * dx + dy * dy < best) {
                    best = dx * dx + dy * dy;
                    best_track = static_cast<int>(k);
                }
            }
        }
    }
    // Draw each line without sub-pixel points (no miter spikes zoomed out).
    auto simplified = [](const std::vector<ImVec2> &l) {
        std::vector<float> xs, ys;
        for (const ImVec2 &p : l) {
            xs.push_back(p.x);
            ys.push_back(p.y);
        }
        std::vector<ImVec2> out;
        for (size_t k : maptiles::simplify_screen(xs, ys, maptiles::kMinSegmentPx))
            out.push_back(l[k]);
        return out;
    };
    for (size_t k = 0; k < lines.size(); ++k)
        if (lines[k].size() > 1 && static_cast<int>(k) != best_track) {
            const std::vector<ImVec2> l = simplified(lines[k]);
            const ImU32 col = k < colours.size() ? colours[k] : IM_COL32(140, 140, 140, 200);
            f.dl->AddPolyline(l.data(), static_cast<int>(l.size()), col, ImDrawFlags_None, 2.0f);
        }
    std::string clicked;
    if (best_track >= 0) {
        const std::vector<ImVec2> l = simplified(lines[static_cast<size_t>(best_track)]);
        if (l.size() > 1)
            f.dl->AddPolyline(l.data(), static_cast<int>(l.size()), IM_COL32(255, 160, 0, 255),
                              ImDrawFlags_None, 4.0f);  // hovered: orange, on top
        hovered_id = tracks[static_cast<size_t>(best_track)]->id;
        if (f.clicked)
            clicked = hovered_id;
    }
    message = end_map(m, f);
    return clicked;
}

