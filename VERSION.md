# Version history

The current version is in [`VERSION`](VERSION). This file says what each
version holds, and has one row for each push to `main` or merged pull request
that changed what OpenRider does or how far it can be relied on.

## How versions are numbered

Versions read `0.RUN.LANDING`.

- **`RUN`** goes up when a new run of related work starts. It does not by
  itself mean a breaking change, and a run can hold one: the plugin ABI changed
  in 0.2.3.
- **`LANDING`** goes up for each later landing on `main` in that run, whether
  it adds a feature or hardens what exists.
- **A landing that changes only documentation or the project model** gets no
  number and stays on the version before it.
- **OpenRider stays at 0.x** while it is a prototype. CI also covers macOS only;
  the other platforms are run by hand ([docs/platforms.md](docs/platforms.md)).

Each new row gets a matching `v<version>` tag on its commit.

## Reconstructed history

Versions 0.2.0 to 0.5.1 were worked out afterwards, on 2026-11-05, from the
history of `main`. They were not releases at the time, and `VERSION` still read
0.1.1 in all of those commits. Only 0.1.0 and 0.1.1 were released as such, with
tags made then. Dates are UTC, from GitHub's record of when each landing reached
`main`.

### 0.5: Windows on every toolchain

| Version | Date | Commit | Landing | What changed |
|---|---|---|---|---|
| 0.5.1 | 2026-11-04 | `cb35943` | PR #11 | A GPX date Windows can't convert no longer crashes MSVC builds; `scale.tst` fixes; test directories removed on exit |
| 0.5.0 | 2026-11-03 | `0d5254a` | PR #10 | Six Windows configurations, including the first MSVC application; `gui.tst` opens the real window; CI fetches the published tracks |

### 0.4: GPX, Linux and BSD, map projections

| Version | Date | Commit | Landing | What changed |
|---|---|---|---|---|
| 0.4.2 | 2026-10-26 | `74868c4` | PR #9 | GPX routes as untimed outings; times never go backwards; published test tracks; `scale.tst` on 10,000 tracks |
| 0.4.1 | 2026-10-26 | `ee9cee7` | push | Tile downloads use the system's certificates; map layers logged as they are added |
| 0.4.0 | 2026-10-23 | `601df83` | PR #8 | GPX files alongside GarminDB; builds on Linux, FreeBSD and NetBSD; map projections and reprojection; plugin ABI 2.1 |

### 0.3: first Windows build, rests

| Version | Date | Commit | Landing | What changed |
|---|---|---|---|---|
| 0.3.1 | 2026-10-16 | `58813b8` | PR #7 | Rests: pauses at a place are left out of segments and cut them |
| 0.3.0 | 2026-10-16 | `688ad5a` | PR #6 | Windows build with mingw-w64, not yet launched there; explicit plugin exports; plugin test tool |

### 0.2: segment networks and the plugin SDK

| Version | Date | Commit | Landing | What changed |
|---|---|---|---|---|
| 0.2.4 | 2026-10-14 | `059b49b` | PR #5 | Analysis on recorded points; stops decided by recorded speed; track filters on the overview; review fixes |
| 0.2.3 | 2026-10-13 | `cc963b8` | PR #4 | Plugin ABI 2.0 (behaviour change): plugins built for ABI 1 are refused; plugin SDK, so plugins no longer link libcore; plugin interface specification |
| 0.2.2 | 2026-10-13 | `9923c7a` | PR #3 | `AppController` split into model domains, segment plugins split by stage; the app behaves as before |
| 0.2.1 | 2026-10-13 | `3eb8e6f` | PR #2 | CI builds and tests on macOS |
| 0.2.0 | 2026-10-13 | `04580ee` | PR #1 | Segment identification as plugins, heat map and track matching; segment comparison panel |

## Released versions

| Version | Date | Commit | Landing | What changed |
|---|---|---|---|---|
| 0.1.1 | 2026-10-13 | `75899f9` | push | Strict MVC, dependencies framework, coding rules; segment network on the overview with an adjustable trace width |
| 0.1.0 | 2026-10-12 | `822e546` | push | First prototype: GarminDB outings on a map, with stacked graphs and statistics |

## Plugin ABI

The version of the C interface plugins are built against
(`include/openrider/plugin.h`), and the first version of OpenRider with it:

| Plugin ABI | OpenRider |
|---|---|
| 2.1 | 0.4.0 |
| 2.0 | 0.2.3 |
| 1 | 0.1.0 |
