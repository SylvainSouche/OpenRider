# To do on real machines

What cannot be checked from the macOS development machine, its Linux
containers and its BSD guests (no display, no Windows, arm64 only). Each item
says what to run and what to look for. Results go in
[docs/platforms.md](docs/platforms.md).

## Windows (a PC or a Windows VM)

Last built at the Windows port (mingw-w64, 2026-10-15); everything since is
untested there: the reprojection module, the WMTS plugin, the map
projections, the extent rules, and the fixes below written for Windows.

- [ ] **Build** with mingw-w64 (`bmake`, `bmake test`). In particular:
  - `libmaptiles.m/src/capabilities.cpp` now creates directories with
    `core::MakeDirs` (POSIX `mkdir(path, mode)` does not compile with MinGW);
  - `openrider.m/src/file_dialog.cpp`: the new `GetOpenFileNameW` dialog,
    linked with `-lcomdlg32` (`openrider.m/makefile`);
  - `libcore.m/tests/paths_test.cpp` expects `paths_test.exe` beside itself.
- [ ] **Tests**: the whole suite, `perf.tst` included (its budgets are in
  calibration units: they should hold).
- [ ] **File > Import GPX**: the Windows dialog opens, several files can be
  chosen, names with accents survive (UTF-16 to UTF-8), Cancel imports
  nothing.
- [ ] **GarminDB**: found in `%USERPROFILE%\HealthData` (the plugin now falls
  back on `USERPROFILE` when `HOME` is unset), or where `garmindb.json` says.
- [ ] **User folders**: `%APPDATA%\OpenRider` (layers, `crs.txt`,
  `outing_types.json`, `gpx`), `%LOCALAPPDATA%\OpenRider\Cache` and `\Logs`.
- [ ] **GUI smoke**: start, list fills, an outing on the map, OpenTopoMap tiles,
  Kartverket and IGN through WMTS, the Projection dropdown (Lambert-93 near
  Toulon, UTM 33N over Norway), warped tiles drawn without seams.
- [ ] Optional: the `msvc` toolchain (Bmake It's `TOOLCHAIN=msvc`, EWDK).

## Linux desktop (a real machine: X11 and Wayland)

The containers build and test everything but never open a window.

- [ ] **GUI smoke** under X11 and under Wayland (GLFW), Mesa and, if at hand,
  a proprietary driver: the window opens, fonts and icons render, tiles show.
- [ ] **Warped tiles** (`DrawTiles`: textured triangle meshes through
  `ImDrawList::PrimReserve`): no gaps or seams between tiles in UTM 33N and
  Lambert-93; a full view of tiles stays fluid.
- [ ] **File > Import GPX** with zenity (GNOME and others) and with kdialog
  only (KDE, zenity uninstalled); with neither, the menu reports
  "No file browser found".
- [ ] **User folders** follow XDG: `~/.local/share/OpenRider`,
  `~/.cache/OpenRider`, `~/.local/state/OpenRider/logs`, and the
  `XDG_*_HOME` variables when set. (Files left by earlier builds under
  `~/Library/...` are not moved: delete them.)
- [ ] **HiDPI**: map and graphs sharp on a scaled display.

## FreeBSD and NetBSD (real machines, or guests with a display)

The QEMU guests build and test (arm64 only) but have no display.

- [ ] **GUI smoke** on X11 with Mesa (FreeBSD: `pkg install xorg mesa-dri`;
  NetBSD: base X11): as for Linux.
- [ ] **Launched from another directory** (e.g. `cd /tmp && /path/to/bin/openrider`):
  the bundled plugins and `share/` are found. The binary is now located
  through `sysctl(KERN_PROC_PATHNAME)`; before, the BSDs had no `/proc`
  and fell back to the working directory. `paths_test` checks the function;
  only a real launch checks the application.
- [ ] **File > Import GPX** with zenity (now used on the BSDs too).
- [ ] **amd64**: build and test on FreeBSD and NetBSD amd64 (no guest on the
  arm64 Mac; QEMU stalls emulating them).
- [ ] **FreeBSD with gcc**: the C++ tests cannot link against base's `atf-c++`
  (libc++). Try an `atf` built with gcc and libstdc++ (ports, `USE_GCC`).

## macOS (this machine, in the application)

- [ ] **In-app check** of the projection work: the dropdown (Web Mercator and
  the systems native to the offered maps), warped tiles, pan and zoom in UTM
  33N and Lambert-93, the view kept inside Kartverket's Norway (Bergen
  reachable), Toulon staying at Toulon in Lambert-93.
- [ ] **Screenshots** of the main views for the README.

## Needs a person rather than a machine

- [ ] **IGN terms** for SCAN 25 through the Géoplateforme (CREDITS.md: "to be
  verified"), and whether the `ign_scan_ws` key may ship.
- [ ] **EPSG terms of use** for the definitions in `crs.txt` (credited; the
  exact acknowledgement wanted to be confirmed).
- [ ] **A P1 checklist**: which confirmed requirements make P1 done (the model
  lists 47 confirmed leaves without an implementation), before a 0.2.0 tag.
- [ ] **Large library**: library scan and segment networks timed on a real
  library of several hundred outings (last measured by hand: about 9 s heat
  map, 23 s matching); a perf case if it can be made synthetic.
- [ ] **Segment regression data**: an anonymised or synthetic reference with
  its expected network, so that topology changes fail a test.
