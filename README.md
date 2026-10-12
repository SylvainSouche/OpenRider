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

## Specification

The requirements, decisions and their history live in `project-model/`
(discovery-driven development). `project-model/index.md` is the entry point.

## Licence

BSD-3-Clause — see `LICENSE`. Third-party credits: `CREDITS.md`.
