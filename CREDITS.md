# Credits

OpenRider is distributed under the BSD-3-Clause licence (see `LICENSE`).
It builds on the following third-party components and data sources. Full
licence texts are in `licenses/`. The same list is shown in the
application's Help → About dialog; keep both in sync.

## Libraries and assets

| Component | Version | Licence | Licence text | How it is obtained |
|---|---|---|---|---|
| [Dear ImGui](https://github.com/ocornut/imgui) — Omar Cornut and contributors | 1.92.9b | MIT | `licenses/imgui-LICENSE.txt` | fetched at build time |
| [ImPlot](https://github.com/epezent/implot) — Evan Pezent | 1.0 | MIT | `licenses/implot-LICENSE.txt` | fetched at build time |
| [cJSON](https://github.com/DaveGamble/cJSON) — Dave Gamble and contributors | 1.7.19 | MIT | `licenses/cjson-LICENSE.txt` | fetched at build time |
| [SQLite](https://sqlite.org) | 3.54.0 | public domain | — | fetched at build time |
| [stb_image](https://github.com/nothings/stb) — Sean Barrett | commit 013ac3b | MIT or public domain | `licenses/stb-LICENSE.txt` | fetched at build time |
| [Phosphor icons](https://phosphoricons.com) — Phosphor Icons | 2.1.2 | MIT | `licenses/phosphor-LICENSE.txt` | fetched at build time (regular-weight font only is installed) |
| [GLFW](https://www.glfw.org) — Marcus Geelnard, Camilla Löwy | 3.4 | zlib | `licenses/glfw-LICENSE.md` | system library |
| [libcurl](https://curl.se) — Daniel Stenberg and contributors | 8.22 | curl (MIT-like) | system package | system library |
| [libtiff](https://libtiff.gitlab.io/libtiff/) — Sam Leffler, Silicon Graphics | 4.7 | libtiff (BSD-like) | system package | system library |

## Data sources

| Source | Licence | Use |
|---|---|---|
| [GarminDB](https://github.com/tcgoetz/GarminDB) output | GPL-2.0 (program); read as data only, not linked | activity input |
| [OpenTopoMap](https://opentopomap.org) | CC-BY-SA | default map layer — attribution shown on the map: "Map data: © OpenStreetMap contributors, SRTM \| Map representation: © OpenTopoMap (CC-BY-SA)" |
| [IGN SCAN 25 Touristique](https://geoservices.ign.fr) via the Géoplateforme WMTS | IGN terms (to be verified) | optional map layer — attribution shown on the map: "© IGN — SCAN 25® Touristique (Géoplateforme)" |
| [Kartverket Topo](https://www.kartverket.no/api-og-data/kartgrunnlag-fastlands-norge) via Kartverket's open WMTS cache (`cache.kartverket.no`) | CC BY 4.0 | optional map layer for Norway — attribution shown on the map: "© Kartverket (CC BY 4.0)" |
<!-- @impl 0f87-6ad8-592a-f21b -->
