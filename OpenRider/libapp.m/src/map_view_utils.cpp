#include "app/map_view_utils.h"

#include "maptiles/mercator.h"

namespace app {

const TrackStyleOption kTrackStyles[6] = {
    {"Solid colour", openrider::sdk::SeriesKind::Altitude},  // kind unused for solid
    {"Speed", openrider::sdk::SeriesKind::Speed},
    {"Heart rate", openrider::sdk::SeriesKind::HeartRate},
    {"Altitude", openrider::sdk::SeriesKind::Altitude},
    {"Grade", openrider::sdk::SeriesKind::Grade},
    {"Vertical speed", openrider::sdk::SeriesKind::VerticalSpeed},
};

void MapOrigin(const MapViewModel &view_model, double &left, double &top)
{
    const MapLayerInfo *layer = view_model.Layer();
    const MapCamera &camera = view_model.Camera();
    double centre_x = 0.0, centre_y = 0.0;
    maptiles::LonlatToWorld(camera.lon, camera.lat, camera.zoom, layer ? layer->tile_size : 256, centre_x,
                            centre_y);
    left = centre_x - view_model.width / 2.0;
    top = centre_y - view_model.height / 2.0;
}

maptiles::GeoBox MapViewBox(const MapViewModel &view_model)
{
    maptiles::GeoBox box{-180, -85, 180, 85};
    const MapLayerInfo *layer = view_model.Layer();
    if (!layer || view_model.width <= 0.0f)
        return box;
    double left, top;
    MapOrigin(view_model, left, top);
    const int zoom = view_model.Camera().zoom;
    maptiles::WorldToLonlat(left, top + view_model.height, zoom, layer->tile_size, box.west, box.south);
    maptiles::WorldToLonlat(left + view_model.width, top, zoom, layer->tile_size, box.east, box.north);
    return box;
}

std::vector<int> MapOfferedLayers(const MapViewModel &view_model, const AppModel &model)
{
    std::vector<int> offered;
    const maptiles::GeoBox view = MapViewBox(view_model);
    std::vector<double> lons, lats;
    if (view_model.mode == MapMode::Outing)
        for (const or_point &point : model.points) {
            lons.push_back(point.lon);
            lats.push_back(point.lat);
        }
    for (int i = 0; i < static_cast<int>(view_model.layers.size()); ++i)
        if (i == view_model.current ||
            maptiles::LayerOffered(view_model.layers[static_cast<size_t>(i)].extent, view, lons, lats))
            offered.push_back(i);
    return offered;
}

} // namespace app
