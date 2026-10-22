# Maps and projections

How OpenRider shows maps: the background layers, the coordinate systems a
map can be shown in, and how tiles and tracks get there. For the plugin
interface behind it, see [plugin-api.md](plugin-api.md) §5.5.

## Using it

Above the map, **Background** picks the layer and **Projection** the
coordinate system it is shown in.

- **Web Mercator** is the default, as on web maps. When the layer's
  provider also publishes its map in a national grid that covers the view
  (Kartverket in UTM over Norway, IGN in Lambert-93 over France), the tiles
  come from that grid and are reprojected to Web Mercator.
- The other entries are the coordinate systems the provider declares:
  Kartverket Topo offers ETRS89 / UTM 32N, 33N and 35N; IGN SCAN 25 offers
  Lambert-93. In one of them, the provider's tiles are shown as published.
  A layer published in Web Mercator only (OpenTopoMap) has no other entry.
- Switching keeps the scale on the ground. Near the polar circle the zoom
  figure changes noticeably — Web Mercator stretches the map by 1/cos(latitude),
  2.7 times at 68° N — while the map on screen keeps its size.
- Zoom in steps of a quarter level with the wheel (half a level with the
  `+` and `−` buttons); the gauge between them shows where the zoom lies in
  the layer's range.

Tracks, segments and the hovered section of a histogram bar are drawn in the
same projection as the map. A track too far from a national grid's area (more
than 60° of longitude from a UTM zone's central meridian) is not drawn in it.

## Layers

Each layer is a JSON file in `share/openrider/layers/` (shipped) or in the
user's `layers/` directory (`~/Library/Application Support/OpenRider/layers`
on macOS). Two protocols are built in.

**`xyz`** — tiles addressed by a URL template in Web Mercator:

```json
{
  "id": "opentopomap", "name": "OpenTopoMap", "protocol": "xyz",
  "url": "https://{s}.tile.opentopomap.org/{z}/{x}/{y}.png", "servers": ["a", "b", "c"],
  "min_zoom": 1, "max_zoom": 17, "extent": [-180, -85.05, 180, 85.05],
  "attribution": "..."
}
```

**`wmts`** — an OGC WMTS service, read from its capabilities:

```json
{
  "id": "kartverket_topo", "name": "Kartverket Topo (Norway)", "protocol": "wmts",
  "capabilities": "https://cache.kartverket.no/v1/wmts/1.0.0/WMTSCapabilities.xml",
  "layers": ["topo"],
  "min_zoom": 0, "max_zoom": 18, "extent": [3.78, 57.64, 31.99, 71.57],
  "attribution": "© Kartverket (CC BY 4.0)"
}
```

- `layers` names the WMTS layers making up the map, in order of preference.
  Each brings its tile matrix sets, one per coordinate system: IGN publishes
  SCAN 25 as one layer in Web Mercator and another in Lambert-93, so its file
  lists both.
- Tiles are requested through the layer's REST template, or as KVP
  `GetTile` requests when the service has none.
- `headers` (both protocols) adds HTTP headers, an API key for instance.
- `min_zoom` and `max_zoom` bound the zoom, in Web Mercator levels;
  `extent` (WGS 84 degrees) decides when the layer is offered.

The capabilities are downloaded when the layer is first shown, then at most
once a week; they are kept beside the tiles and used offline. Tiles are
cached by grid (`<cache>/<layer id>@<tile matrix set>/…`; the Web Mercator
grid of an `xyz` layer keeps `<cache>/<layer id>/…`), 2 GB in all, least
recently used first out.

## Coordinate systems

The systems OpenRider knows are declared in `share/openrider/crs.txt`, and
the user may add more in a `crs.txt` of their own beside `layers/`. One
line each:

```
EPSG:2154 | RGF93 v1 / Lambert-93 | -9.86,41.15,10.38,51.56 | +proj=lcc +lat_0=46.5 +lon_0=3 +lat_1=49 +lat_2=44 +x_0=700000 +y_0=6600000 +ellps=GRS80 +units=m
```

code, name, area of use (west, south, east, north in WGS 84 degrees), and the
definition as `projinfo -o PROJ EPSG:<code>` or epsg.io prints it. A
provider's grid in a system missing from these files is ignored.

The shipped file holds 163 systems: the national grids of the United
Kingdom, Ireland, France, Belgium, the Netherlands, Luxembourg, Switzerland,
Austria, Italy, Portugal, Poland, Czechia and Slovakia, Hungary, Slovenia,
Croatia, Romania, Bulgaria, Greece, Sweden, Finland, Iceland and the Baltic
states; LAEA and LCC Europe; ETRS89 and WGS 84 UTM zones.

Definitions are understood, not approximated: the projection methods are
`longlat`, `merc` and `webmerc`, `tmerc` and `utm`, `lcc` (one or two
standard parallels), `sterea`, `somerc`, `krovak` and `laea`, on any
ellipsoid; anything else — another method, a grid shift (`+nadgrids`),
units other than metres, a prime meridian other than Greenwich — is refused
with the line number. Old national datums (OSGB36, Amersfoort, BD72, CH1903+,
MGI, S-JTSK, HD72, Pulkovo 1942(58), LUREF, GGRS87) are shifted to WGS 84
with seven-parameter Helmert transformations (`+towgs84`, EPSG's best: 1 to
6 m, enough for display); ETRS89 and its national realisations are taken as
WGS 84, from which they differ by less than a metre.

The projections agree with PROJ 9.7 to a tenth of a millimetre on every
system shipped, including far from a UTM zone's central meridian
(`libreproj.m/tests/projection_test.cpp`).

## How it works

`libreproj` is the reprojection module. It depends on no other module and
on no third-party library.

- **Projections**: one class per method, in double precision
  (transverse Mercator by Krüger's series after Karney, the others by the
  formulas of EPSG guidance note 7-2 and swisstopo); `CrsRegistry` reads the
  declarations; `Transformer` converts between two systems through WGS 84,
  batches spread over the CPU's cores.
- **Vertices** (tracks, segments): `GridProjector` projects through a grid
  of exactly transformed nodes over the view, refined until bilinear
  interpolation errs by less than a quarter of a pixel — about 7 ns a point
  instead of 100 to 300 ns. It falls back to exact transformation outside
  the grid, and when zoomed out too far for a grid to be that accurate.
- **Rasters** (tiles): `BuildWarpMesh` turns a tile into a grid of textured
  triangles whose nodes are transformed exactly, refined until linear
  interpolation errs by less than a quarter of a pixel (one or two cells per
  side for a tile over the Lofoten). The GPU draws the triangles and samples
  the texture: hardware reprojection with any graphics API and no shader. A
  tile in the display's own system is a single quad. `WarpRaster` does the
  same on the CPU (exports, tests).

In the application (`libapp`), `MapGeometry` holds the display system, the
scale and the viewport for one frame and converts between WGS 84, display
coordinates and screen pixels; the camera stays a WGS 84 centre and a zoom,
so it means the same in every system. `SourceGrid()` picks the grid tiles
come from — native to the display, else the local grid whose system covers
the view, else Web Mercator — and `VisibleTiles()` the level whose pixels
best match the screen's and the tiles covering the view, within the layer's
limits.

## Performance

`perf.tst` measures the hot paths against budgets expressed in units of a
fixed calibration workload run on the same machine, so the same budgets hold
on a fast laptop and under emulation; it reports each figure and fails only
on a gross regression. Reference figures (Apple M-series, macOS):

| Path | Measured | Budget |
|---|---|---|
| Transform a million points, WGS 84 → Lambert-93 / UTM 33N / RD New (datum shift), batched over 10 cores | 13 / 28 / 75 ns a point | 30 / 30 / 60 units |
| Frame geometry, projection grid built | 0.2–0.5 ms | 1 unit |
| A million track points to screen (`GridProjector`) | 7 ns a point | 4 units |
| Warp meshes of 300 tiles (a full view) into UTM 33N | 1–1.5 ms | 1 unit |
| CPU raster warp, 1024 × 1024 | 3 ms | 4 units |

A unit is the calibration workload's time: 9 ms there. Budgets leave room
for machines with fewer cores, as the BSD guests have.
