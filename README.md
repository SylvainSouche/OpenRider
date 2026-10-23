# OpenRider

Manage, analyse and display outings (hiking, running, skiing, cycling…)
recorded with Garmin devices. OpenRider reads [GarminDB](https://github.com/tcgoetz/GarminDB)'s
databases and files (read-only) and shows each outing on a map with
stacked graphs and statistics.

Status: prototype P1 in progress.

## Build

Requires [Bmake It](https://github.com/SylvainSouche/bmake-it) and, on
macOS, these MacPorts packages: `bmake glfw curl tiff atf kyua pkgconfig`.

```sh
export MAKESYSPATH=<bmake-it>/mk:/opt/local/share/mk
bmake            # build everything
bmake test       # unit tests (ATF/Kyua)
./build/macos-arm64/bin/openrider
```

Third-party sources (Dear ImGui, …) are fetched at build time and checked
against each module's `distinfo`; they are never committed.

Linux (glibc and musl), FreeBSD and NetBSD build and pass the tests too: the
packages each needs, the results and how to reproduce them are in
[docs/platforms.md](docs/platforms.md).

Maps, layers (XYZ and WMTS), the coordinate systems a map can be shown in
and how reprojection works: [docs/maps.md](docs/maps.md).

## Architecture

Two Bmake It frameworks:

- **OpenRiderDependencies** — every third-party module (Dear ImGui, ImPlot,
  cJSON, SQLite, stb_image, Phosphor icons fetched at build time; GLFW,
  libcurl, libtiff from the system).
- **OpenRider** — the project's own code:

| Module | Role |
|---|---|
| `include/openrider/` | Plugin ABI (pure C, versioned): input, tiles, filters, segments |
| `libcore` | Domain logic: outings, activity types, statistics, motion, series, overview, segment networks (built by segments plugins), plugin loading, log |
| `libmaptiles` | Map tiles: tile grids (Web Mercator and those a WMTS service publishes), capabilities download, polite fetching, disk cache, decoding, coverage |
| `libreproj` | Reprojection, independent of the rest: coordinate systems declared as text (`share/common/openrider/crs.txt`: EPSG code, area of use, PROJ string; 163 systems, the European national grids among them), projection methods and 7-parameter datum shifts coded in double precision, batch vertex transforms, raster warping as a GPU triangle mesh or on the CPU |
| `libapp` | MVC without GUI: `AppModel`, changed only by its domains (library, query, selected outing, segments, comparison, session) owned and routed by `AppController`; view models with their view controllers; intents and dispatcher |
| `openrider` | Views (Dear ImGui), read-only: draw from the model and view models, report intents |
| `input_garmindb`, `input_gpx`, `tiles_xyz`, `tiles_wmts`, `filter_jitter`, `segments_heatmap`, `segments_matching` | Plugins, loaded at run time from `plugins/<kind>/`; `tiles_wmts` reads a service's WMTS capabilities and offers each coordinate system it publishes |

Strict MVC: views receive `const` models and push intents; the dispatcher
routes each intent to the one controller owning it; only `AppController`
changes the model -- through its domains, each handling the intents about one part of it -- and only a view controller changes its view model.

## Coding rules

<!-- @impl 0f87-6acd-d7ea-421f -->
<!-- @impl 0f87-6acd-d7ea-db3a -->

- Classes, methods and free functions in UpperCamelCase; fields, locals and
  parameters in lowercase snake_case (checked by `.clang-tidy`). The C
  plugin ABI (`or_*`) keeps C naming.
- One header and one source per class (`library.h`/`library.cpp`, ...).
  Free functions live in module-level utility files (`outing_selection.h`,
  `overview.h`, `map_view_utils.h`, `style.h`, ...); plain data structs sit
  in the header of the class or module owning them.
- Every class, method and function has a Doxygen comment (`bmake docs`);
  complex methods are also explained along their implementation.
- Names say what things are; short names only where usage accepts them
  (loop indices, coordinates, tile `z`/`x`/`y`).

## Specification

The requirements, decisions and their history live in `project-model/`
(discovery-driven development). `project-model/index.md` is the entry point.

## Licence

BSD-3-Clause — see `LICENSE`. Third-party credits: `CREDITS.md`.
