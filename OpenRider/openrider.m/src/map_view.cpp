// Map view: tiles from the map view model, the overview's tracks or the
// selected outing's styled track, attribution. Mouse actions are reported
// as intents (pan, zoom, hover, click, viewport size).

#include "views.h"

#include <GLFW/glfw3.h>

#include "maptiles/mercator.h"
#include "maptiles/screen.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ui {

namespace {

// Points closer than 1.5 px to the last drawn one are skipped: sub-pixel
// GPS zigzags would otherwise become miter spikes on a thick line.
std::vector<ImVec2> Simplified(const std::vector<ImVec2> &points)
{
    std::vector<float> xs, ys;
    for (const ImVec2 &point : points) {
        xs.push_back(point.x);
        ys.push_back(point.y);
    }
    std::vector<ImVec2> kept;
    for (size_t k : maptiles::SimplifyScreen(xs, ys, maptiles::kMinSegmentPx))
        kept.push_back(points[k]);
    return kept;
}

// Blue -> cyan -> green -> yellow -> red.
ImU32 Gradient(double position)
{
    static const float stops[5][3] = {
        {0.10f, 0.30f, 1.00f}, {0.00f, 0.80f, 0.90f}, {0.10f, 0.75f, 0.10f}, {0.95f, 0.85f, 0.05f}, {0.90f, 0.10f, 0.10f}};
    position = std::clamp(position, 0.0, 1.0) * 4.0;
    const int stop = std::min(3, static_cast<int>(position));
    const float fraction = static_cast<float>(position - stop);
    float colour[3];
    for (int k = 0; k < 3; ++k)
        colour[k] = stops[stop][k] + (stops[stop + 1][k] - stops[stop][k]) * fraction;
    return ImGui::ColorConvertFloat4ToU32(ImVec4(colour[0], colour[1], colour[2], 0.95f));
}

// Where and how the map is drawn this frame.
struct Frame {
    ImVec2 origin, end;                 // map region on screen
    double left = 0.0, top = 0.0;       // world-pixel origin of the region
    int zoom = 0, tile_size = 256;      // view zoom and the layer's tile size
    ImDrawList *draw_list = nullptr;
    // Screen position of a WGS84 point.
    ImVec2 At(double lon, double lat) const
    {
        double x, y;
        maptiles::LonlatToWorld(lon, lat, zoom, tile_size, x, y);
        return ImVec2(static_cast<float>(origin.x + x - left), static_cast<float>(origin.y + y - top));
    }
};

// @impl 0f87-6acd-5daf-1652
void DrawTiles(const app::MapViewModel &view_model, TextureMirror &textures, const Frame &frame)
{
    const app::MapLayerInfo *layer = view_model.Layer();
    const int tile_zoom = std::min(frame.zoom, layer->max_zoom);
    const double tile_px = layer->tile_size * std::pow(2.0, frame.zoom - tile_zoom);  // enlarged past the last level
    const int tiles_across = 1 << tile_zoom;
    const double width = frame.end.x - frame.origin.x, height = frame.end.y - frame.origin.y;
    for (int row = std::max(0, static_cast<int>(std::floor(frame.top / tile_px)));
         row <= std::min(tiles_across - 1, static_cast<int>(std::floor((frame.top + height) / tile_px))); ++row)
        for (int column = static_cast<int>(std::floor(frame.left / tile_px)); column <= static_cast<int>(std::floor((frame.left + width) / tile_px));
             ++column) {
            const maptiles::TileKey key{tile_zoom, ((column % tiles_across) + tiles_across) % tiles_across, row};
            auto decoded = view_model.tiles.find(key);
            if (decoded == view_model.tiles.end())
                continue;
            const ImVec2 top_left(static_cast<float>(frame.origin.x + column * tile_px - frame.left), static_cast<float>(frame.origin.y + row * tile_px - frame.top));
            const ImVec2 bottom_right(static_cast<float>(top_left.x + tile_px), static_cast<float>(top_left.y + tile_px));
            frame.draw_list->AddImage(static_cast<ImTextureID>(textures.Texture(key, decoded->second)), top_left, bottom_right);
        }
}

// Overview: the outings passing the list's type and date filters, each in
// its activity colour; the one under the mouse in orange, on top.
// @impl 0f87-6acd-5b43-c60a
// @impl 0f87-6acd-6195-c32d
void DrawOverviewTracks(const app::AppModel &model, const app::MapViewModel &view_model, const Frame &frame)
{
    std::map<std::string, size_t> listed;
    for (size_t i : core::SelectOutings(model.outings, model.MapFilter(), core::SortKey::Date, true))
        listed[model.outings[i].raw.id] = i;
    const maptiles::GeoBox box = app::MapViewBox(view_model);
    std::vector<ImVec2> hovered;
    for (const core::OverviewTrack &track : model.tracks) {
        const auto listed_index = listed.find(track.id);
        if (listed_index == listed.end() || track.east < box.west || track.west > box.east || track.north < box.south || track.south > box.north)
            continue;
        std::vector<ImVec2> line;
        for (size_t i = 0; i < track.lat.size(); ++i)
            line.push_back(frame.At(track.lon[i], track.lat[i]));
        line = Simplified(line);
        if (track.id == view_model.hovered_track) {
            hovered = line;
            continue;
        }
        if (line.size() > 1)
            frame.draw_list->AddPolyline(line.data(), static_cast<int>(line.size()),
                              ColourU32(model.types.Colour(model.outings[listed_index->second].type), 200), ImDrawFlags_None, 2.0f);
    }
    if (hovered.size() > 1)
        frame.draw_list->AddPolyline(hovered.data(), static_cast<int>(hovered.size()), IM_COL32(255, 160, 0, 255),
                          ImDrawFlags_None, 4.0f);
}

// @impl 0f87-6acd-d9d9-da69
// Overview with segments: the network of each activity group. Shared
// segments coloured and thickened by their number of routes (blue: two,
// red: the most travelled), single-route segments thin and faint, nodes as
// dots, the hovered segment in orange on top; a legend of the colour scale.
void DrawSegmentNetworks(const app::AppModel &model, const app::MapViewModel &view_model, const Frame &frame)
{
    size_t most_routes = 2;
    for (const core::SegmentGroup &group : model.segment_groups)
        for (const core::NetworkSegment &segment : group.network.segments)
            most_routes = std::max(most_routes, segment.route_ids.size());
    auto line_of = [&](const core::NetworkSegment &segment) {
        std::vector<ImVec2> line;
        for (size_t i = 0; i < segment.lat.size(); ++i)
            line.push_back(frame.At(segment.lon[i], segment.lat[i]));
        return Simplified(line);
    };
    // Single-route segments first, so that shared ones are drawn over them.
    for (int pass = 0; pass < 2; ++pass)
        for (const core::SegmentGroup &group : model.segment_groups)
            for (const core::NetworkSegment &segment : group.network.segments) {
                if (segment.Shared() != (pass == 1))
                    continue;
                const std::vector<ImVec2> line = line_of(segment);
                if (line.size() < 2)
                    continue;
                if (!segment.Shared()) {
                    frame.draw_list->AddPolyline(line.data(), static_cast<int>(line.size()), IM_COL32(60, 60, 60, 110),
                                                 ImDrawFlags_None, 1.5f);
                    continue;
                }
                const double share = most_routes > 2 ? double(segment.route_ids.size() - 2) / double(most_routes - 2) : 0.0;
                frame.draw_list->AddPolyline(line.data(), static_cast<int>(line.size()), Gradient(share), ImDrawFlags_None,
                                             2.5f + 3.0f * static_cast<float>(share));
            }
    for (const core::SegmentGroup &group : model.segment_groups)
        for (const core::NetworkNode &node : group.network.nodes)
            frame.draw_list->AddCircleFilled(frame.At(node.lon, node.lat), 2.5f, IM_COL32(30, 30, 30, 200));
    if (view_model.hovered_group >= 0 && view_model.hovered_group < static_cast<int>(model.segment_groups.size())) {
        const core::SegmentNetwork &network = model.segment_groups[static_cast<size_t>(view_model.hovered_group)].network;
        if (view_model.hovered_segment >= 0 && view_model.hovered_segment < static_cast<int>(network.segments.size())) {
            const core::NetworkSegment &segment = network.segments[static_cast<size_t>(view_model.hovered_segment)];
            const std::vector<ImVec2> line = line_of(segment);
            if (line.size() > 1)
                frame.draw_list->AddPolyline(line.data(), static_cast<int>(line.size()), IM_COL32(255, 140, 0, 255),
                                             ImDrawFlags_None, 6.0f);
            for (int end : {segment.from, segment.to})
                if (end >= 0 && end < static_cast<int>(network.nodes.size()))
                    frame.draw_list->AddCircleFilled(frame.At(network.nodes[static_cast<size_t>(end)].lon,
                                                              network.nodes[static_cast<size_t>(end)].lat),
                                                     5.0f, IM_COL32(255, 140, 0, 255));
        }
    }
    // Legend: routes per shared segment.
    const ImVec2 legend_origin(frame.origin.x + 8.0f, frame.origin.y + 8.0f);
    const float bar_width = 120.0f, bar_height = 8.0f;
    frame.draw_list->AddRectFilled(ImVec2(legend_origin.x - 4, legend_origin.y - 4),
                                   ImVec2(legend_origin.x + bar_width + 4, legend_origin.y + bar_height + 34),
                                   IM_COL32(255, 255, 255, 210), 3.0f);
    for (int k = 0; k < 24; ++k)
        frame.draw_list->AddRectFilled(ImVec2(legend_origin.x + bar_width * k / 24.0f, legend_origin.y + 14),
                                       ImVec2(legend_origin.x + bar_width * (k + 1) / 24.0f, legend_origin.y + 14 + bar_height),
                                       Gradient(k / 23.0));
    frame.draw_list->AddText(legend_origin, IM_COL32(40, 40, 40, 255), "Routes per segment");
    char text[32];
    std::snprintf(text, sizeof text, "2");
    frame.draw_list->AddText(ImVec2(legend_origin.x, legend_origin.y + 16 + bar_height), IM_COL32(40, 40, 40, 255), text);
    std::snprintf(text, sizeof text, "%zu", most_routes);
    frame.draw_list->AddText(ImVec2(legend_origin.x + bar_width - ImGui::CalcTextSize(text).x, legend_origin.y + 16 + bar_height),
                             IM_COL32(40, 40, 40, 255), text);
}

// Comparison: the segment, wide and pale, and each shown passage in its
// colour on top; while the mouse is over the graphs, a dot in each shown
// passage's colour where it is at the cursor.
void DrawComparedPassages(const app::AppModel &model, const app::ComparisonViewModel &view_model, const Frame &frame)
{
    const core::NetworkSegment &segment = model.comparison.segment;
    std::vector<ImVec2> line;
    for (size_t i = 0; i < segment.lat.size(); ++i)
        line.push_back(frame.At(segment.lon[i], segment.lat[i]));
    if (line.size() > 1)
        frame.draw_list->AddPolyline(line.data(), static_cast<int>(line.size()), IM_COL32(40, 90, 200, 90), ImDrawFlags_None,
                                     10.0f);
    for (size_t r = 0; r < model.comparison.runs.size(); ++r) {
        if (r >= view_model.shown.size() || !view_model.shown[r])
            continue;
        std::vector<ImVec2> passage;
        for (const or_point &point : model.comparison.runs[r].points)
            if (!std::isnan(point.lat) && !std::isnan(point.lon))
                passage.push_back(frame.At(point.lon, point.lat));
        passage = Simplified(passage);
        if (passage.size() > 1)
            frame.draw_list->AddPolyline(passage.data(), static_cast<int>(passage.size()),
                                         ImGui::ColorConvertFloat4ToU32(RunColour(r)), ImDrawFlags_None, 2.0f);
    }
    // Where each shown passage is at the graphs' cursor.
    if (!view_model.hovering)
        return;
    for (size_t r = 0; r < model.comparison.runs.size(); ++r) {
        const app::ComparedRun &run = model.comparison.runs[r];
        if (r >= view_model.shown.size() || !view_model.shown[r] || run.points.empty())
            continue;
        const or_point &point = run.points[app::PassagePointAt(run, view_model.xaxis, view_model.hover_x)];
        if (std::isnan(point.lat) || std::isnan(point.lon))
            continue;
        const ImVec2 at = frame.At(point.lon, point.lat);
        frame.draw_list->AddCircleFilled(at, 6.0f, IM_COL32(255, 255, 255, 255));
        frame.draw_list->AddCircleFilled(at, 4.5f, ImGui::ColorConvertFloat4ToU32(RunColour(r)));
    }
}

// Outing: one colour, or coloured per segment by the chosen series, with a
// legend; the point hovered in the graphs marked.
// @impl 0f87-6acd-32ed-0d05
void DrawOutingTrack(const app::AppModel &model, const ViewModels &view_models, const Frame &frame)
{
    const std::vector<or_point> &points = model.points;
    std::vector<ImVec2> line(points.size());
    std::vector<size_t> valid;
    std::vector<float> xs, ys;
    for (size_t i = 0; i < points.size(); ++i) {
        if (std::isnan(points[i].lat) || std::isnan(points[i].lon))
            continue;
        line[i] = frame.At(points[i].lon, points[i].lat);
        valid.push_back(i);
        xs.push_back(line[i].x);
        ys.push_back(line[i].y);
    }
    std::vector<size_t> drawn;
    for (size_t k : maptiles::SimplifyScreen(xs, ys, maptiles::kMinSegmentPx))
        drawn.push_back(valid[k]);

    const int style = view_models.map.track_style;
    std::vector<double> values;
    double low = 0.0, high = 1.0;
    if (style > 0) {
        core::SeriesSpec spec;
        spec.kind = app::kTrackStyles[style].kind;
        values = core::ComputeSeries(points, view_models.outing.xaxis, spec).y;
        std::vector<double> defined;
        for (double v : values)
            if (!std::isnan(v))
                defined.push_back(v);
        if (defined.size() > 1) {  // 5th-95th percentiles: outliers do not flatten the gradient
            std::sort(defined.begin(), defined.end());
            low = defined[defined.size() * 5 / 100];
            high = std::max(low + 1.0, defined[std::min(defined.size() - 1, defined.size() * 95 / 100)]);
        }
        size_t prev = points.size();
        for (size_t i : drawn) {
            if (prev < points.size())
                frame.draw_list->AddLine(line[prev], line[i],
                              std::isnan(values[i]) ? IM_COL32(120, 120, 120, 220) : Gradient((values[i] - low) / (high - low)),
                              3.5f);
            prev = i;
        }
        const ImVec2 legend_origin(frame.origin.x + 8.0f, frame.origin.y + 8.0f);
        const float bar_width = 120.0f, bar_height = 8.0f;
        frame.draw_list->AddRectFilled(ImVec2(legend_origin.x - 4, legend_origin.y - 4), ImVec2(legend_origin.x + bar_width + 4, legend_origin.y + bar_height + 34), IM_COL32(255, 255, 255, 210), 3.0f);
        for (int k = 0; k < 24; ++k)
            frame.draw_list->AddRectFilled(ImVec2(legend_origin.x + bar_width * k / 24.0f, legend_origin.y + 14), ImVec2(legend_origin.x + bar_width * (k + 1) / 24.0f, legend_origin.y + 14 + bar_height),
                                Gradient(k / 23.0));
        char legend[64];
        std::snprintf(legend, sizeof legend, "%s (%s)", core::SeriesLabel(spec.kind), core::SeriesUnit(spec));
        frame.draw_list->AddText(legend_origin, IM_COL32(40, 40, 40, 255), legend);
        char text[32];
        std::snprintf(text, sizeof text, "%.0f", low);
        frame.draw_list->AddText(ImVec2(legend_origin.x, legend_origin.y + 16 + bar_height), IM_COL32(40, 40, 40, 255), text);
        std::snprintf(text, sizeof text, "%.0f", high);
        frame.draw_list->AddText(ImVec2(legend_origin.x + bar_width - ImGui::CalcTextSize(text).x, legend_origin.y + 16 + bar_height), IM_COL32(40, 40, 40, 255), text);
    } else {
        std::vector<ImVec2> run;
        for (size_t i : drawn)
            run.push_back(line[i]);
        if (run.size() > 1)
            frame.draw_list->AddPolyline(run.data(), static_cast<int>(run.size()), view_models.map.track_colour, ImDrawFlags_None, 3.0f);
    }
    const int hovered = view_models.outing.hover_index;
    if (hovered >= 0 && static_cast<size_t>(hovered) < points.size() && !std::isnan(points[static_cast<size_t>(hovered)].lat)) {
        const ImVec2 centre = line[static_cast<size_t>(hovered)];
        frame.draw_list->AddCircleFilled(centre, 7.0f, IM_COL32(255, 255, 255, 255));
        frame.draw_list->AddCircleFilled(centre, 5.0f, IM_COL32(20, 20, 20, 255));
    }
}

} // namespace

// @impl 0f87-6acd-2f33-0934
void DrawMapView(const app::AppModel &model, const ViewModels &view_models, TextureMirror &textures, float width, float height,
                   app::Intents &out)
{
    const app::MapViewModel &view_model = view_models.map;
    if (view_model.width != width || view_model.height != height)
        out.Push(app::IntentKind::MapViewport, 0, 0, width, height);
    if (!view_model.Layer()) {
        ImGui::TextDisabled("No map layer");
        return;
    }
    Frame frame;
    frame.origin = ImGui::GetCursorScreenPos();
    frame.end = ImVec2(frame.origin.x + width, frame.origin.y + height);
    ImGui::InvisibleButton("##map", ImVec2(width, height));
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const double mouse_x = mouse.x - frame.origin.x - width / 2.0, mouse_y = mouse.y - frame.origin.y - height / 2.0;
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
        const ImVec2 delta = ImGui::GetIO().MouseDelta;
        if (delta.x != 0.0f || delta.y != 0.0f)
            out.Push(app::IntentKind::MapPan, 0, 0, delta.x, delta.y);
    }
    const bool hovered = ImGui::IsItemHovered();
    if (hovered && ImGui::GetIO().MouseWheel != 0.0f)
        out.Push(app::IntentKind::MapZoom, ImGui::GetIO().MouseWheel > 0.0f ? 1 : -1, 0, mouse_x, mouse_y);
    if (hovered || !view_model.hovered_track.empty() || view_model.hovered_segment >= 0)
        out.Push(app::IntentKind::MapMouse, hovered ? 1 : 0, 0, mouse_x, mouse_y);
    const ImVec2 drag = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f);
    if (ImGui::IsItemDeactivated() && drag.x == 0.0f && drag.y == 0.0f)
        out.Push(app::IntentKind::MapClick);
    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        out.Push(app::IntentKind::MapDoubleClick);

    frame.draw_list = ImGui::GetWindowDrawList();
    frame.draw_list->PushClipRect(frame.origin, frame.end, true);
    frame.draw_list->AddRectFilled(frame.origin, frame.end, IM_COL32(200, 202, 205, 255));
    app::MapOrigin(view_model, frame.left, frame.top);
    frame.zoom = view_model.Camera().zoom;
    frame.tile_size = view_model.Layer()->tile_size;
    DrawTiles(view_model, textures, frame);
    if (view_model.mode == app::MapMode::Overview)
        model.show_segments ? DrawSegmentNetworks(model, view_model, frame) : DrawOverviewTracks(model, view_model, frame);
    else if (view_model.mode == app::MapMode::Comparison)
        DrawComparedPassages(model, view_models.comparison, frame);
    else
        DrawOutingTrack(model, view_models, frame);
    // Attribution, always visible while the layer is shown.
    const std::string &attribution = view_model.Layer()->attribution;
    if (!attribution.empty()) {
        const ImVec2 text_size = ImGui::CalcTextSize(attribution.c_str());
        const ImVec2 text_origin(frame.end.x - text_size.x - 6.0f, frame.end.y - text_size.y - 4.0f);
        frame.draw_list->AddRectFilled(ImVec2(text_origin.x - 4.0f, text_origin.y - 2.0f), frame.end, IM_COL32(255, 255, 255, 200));
        frame.draw_list->AddText(text_origin, IM_COL32(40, 40, 40, 255), attribution.c_str());
    }
    frame.draw_list->PopClipRect();
    textures.Prune(view_model);
}

} // namespace ui
