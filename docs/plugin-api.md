# OpenRider plugin interface: architecture and specification

Status: **reviewed**, 2026-10-13; decisions in §7. Nothing here is implemented
unless marked *stable* or *in place*; this document fixes what plugins may
rely on before the SDK is extracted and before new plugin kinds are added.

## 1. Purpose and scope

OpenRider's analysis grows by plugins: data sources, filters, map tiles,
segment identification today; metrics, intervals, export, workflows and
models later (the medium-term scope was set against GoldenCheetah's feature
set: interval discovery, mean-maximal curves, critical power/speed and
W' balance, training load, zones, data-quality tools, user metrics,
export and sync). This document defines

- the **layers** a plugin may depend on, and the one it may not;
- the **exposed domains** of each layer;
- for what exists, the **contract** each domain guarantees, and its
  **stability**;
- for what does not exist yet, a **first specification** (what it must
  offer), to be refined when the first plugin of the kind is written.

It does not add features.

## 2. Layers

| Layer | What | Form | Plugins may use |
|---|---|---|---|
| L0 Plugin ABI | the run-time contract between the application and a plugin binary | C headers `include/openrider/*.h` | yes, it is the only run-time interface |
| L1 Plugin SDK | shared computations and ABI helpers | C++ headers `include/openrider/sdk/*.h` + static library `openrider_sdk` | yes, linked statically into the plugin |
| L2 Application internals | the model, background services, GUI | `local/include/*`, `libcore`, `libmaptiles`, `libapp` | **no** |

Rules:

1. A plugin binary depends at run time on the system and on what it bundles,
   never on an application library. Checked by the `plugin_sealing.tst`
   test module on the built plugins (until the SDK was extracted,
   `filter_jitter`, `segments_heatmap` and `segments_matching` loaded
   `libcore.1.dylib`).
2. The SDK is the single implementation of every rule shared by the
   application and plugins (e.g. the not-moving rule behind moving time and
   the jitter filter). `libcore` uses the SDK; it does not duplicate it.
3. The SDK holds only pure computations: no global state, no threads, no
   I/O, no exceptions, no RTTI. Anything needing the running application
   (logging, progress, settings, caches) is a **host service** of L0.
4. Third-party libraries a plugin needs (SQLite, cJSON, libcurl...) are
   linked by the plugin itself; the SDK does not wrap them.

## 3. Stability levels

| Level | Meaning |
|---|---|
| **Stable** | Frozen for an ABI major version: may only gain members as allowed by the versioning rules (§4). |
| **Provisional** | Usable by in-tree plugins; may change in a minor version, with the in-tree plugins updated together. |
| **Planned** | Specification only; no header yet. |

## 4. Versioning (L0)

Until ABI 1, `OR_PLUGIN_API_VERSION` had to equal the application's
exactly: any change, even an addition, refused every built plugin.

In place since ABI 2.0 (`OR_PLUGIN_API_VERSION` 2, `OR_PLUGIN_API_MINOR` 0):

- `api_version` is the **major** version (refusal on mismatch), `api_minor`
  the **minor** one: a plugin built for minor *n* loads in an application
  of minor >= *n*.
- Every vtable and host table starts with a `size_t struct_size`; functions
  are only ever appended, so a newer application can tell which functions an
  older plugin provides, and the reverse. The loader refuses a table smaller
  than the kind's table as of 2.0 (`core::VtableMinimumSize`).
- Each plugin kind keeps its own kind number; a kind's tables are versioned
  by `struct_size`, not by a separate number.
- The SDK version is the OpenRider version that built it; being static, it
  never has to match the application at run time.

## 5. L0 domains

### 5.1 Plugin descriptor and kinds (`plugin.h`) — stable, in place

`openrider_plugin()` returns a static `or_plugin_info` (version, kind, name,
version string, kind-specific vtable). Kinds: input 1, filters 2, tiles 3,
workflows 4, segments 5. Plugins are found under `plugins/<kind dir>/`, in
the bundled directory then the user's.

The entry function is the only symbol a plugin must export, and it exports it
explicitly: `extern "C" OR_PLUGIN_EXPORT const or_plugin_info
*openrider_plugin(void)`. `OR_PLUGIN_EXPORT` (`plugin.h`) is
`__declspec(dllexport)` on Windows and default visibility elsewhere. A plugin
never relies on the linker exporting everything: on Windows that automatic
export stops as soon as any linked object carries a `dllexport` of its own (a
bundled static library such as cJSON does), and the entry would vanish.

ABI 2.0 adds `api_minor` and `flags`. The first flag,
`OR_PLUGIN_THREAD_SAFE`, says the plugin may be called from several threads
at once; without it the application makes every call to the plugin from one
thread of its own (`core::PluginThread`, `core::CallPlugin`: calls from
other threads are queued to it and wait for their result). This holds for
every kind. All in-tree plugins declare it (GarminDB through SQLite's
serialised mode; the others hold no shared state).

An optional `init(const or_host *host)`, called once after loading (see
5.2), comes in a later minor version, with the first host service a plugin
needs.

### 5.2 Host services (`host.h`) — planned, an ABI 2 minor version

What only the running application can do, offered to plugins as a table of
callbacks handed over by `init()`. Each service is added (appended to the
table, in a minor version) with the first plugin needing it; no plugin
needs one yet.

| Service | Use |
|---|---|
| `log(level, message)` | into the session log, prefixed with the plugin's name |
| `progress(context, fraction, text)` | long operations shown in the status bar |
| `cancelled(context)` | cooperative cancellation (generalises the segments cancel callback) |
| `user_dir(kind)` | the plugin's own cache / data directory |
| `setting(key)` | athlete profile and user settings, read-only (max/resting HR, thresholds, zone limits, weight...) |

To specify when first needed (provisional names): terrain elevation at a
position (elevation correction), tile access through the application's
cache, authorisation flows for online services.

Plugins never write the application's data store; they return results and
the application stores them.

### 5.3 Input (`input.h`) — stable, in place

Outings (`or_outing`) and their points (`or_point`), SI units, NaN / -1 for
missing values, read-only sources.

Contract points to state explicitly (gaps found while writing this):

- **Threading.** The application calls `list_outings` and `load_points` from
  several threads (scan, overview loading, user actions). The plugin
  **declares** whether it is thread-safe (§5.1, `OR_PLUGIN_THREAD_SAFE`).
  - Declared thread-safe: the application may call any function from any
    thread, concurrently, on the same handle.
  - Not declared: the application makes every call to the plugin from a
    single thread of its own (calls from elsewhere are queued to it), so the
    plugin never sees two threads, nor a change of thread.
- **Order.** `list_outings` delivers outings in any order; `load_points`
  delivers points in time order.
- **Identity.** An outing id is stable across runs (the application's own
  data is keyed by it).

Planned additions (ABI 2 minor, by appending): further point channels for
power, cadence types, running dynamics; a capability query (which channels
the source has).

### 5.4 Filters (`filter.h`) — stable for track filters, in place

Track filters (`OR_FILTER_TRACK`): given an outing's points and the declared
parameter values, mark the points to keep. Non-destructive: the application
re-applies the enabled filters over the original points.

Planned kinds (decided in the model, not specified yet):

- **Metadata filters**: read an outing, propose metadata changes with a
  reason (e.g. a type suggestion for a "trail run" in winter with 40 km/h
  descents). Output: suggestions, applied by the user.
- **Point-correcting filters** (data quality: spikes, gaps, elevation):
  return corrected values instead of a keep mask. Elevation correction needs
  the terrain host service.

### 5.5 Tiles (`tiles.h`) — stable, in place

A layer definition (JSON) opened by the protocol's plugin, which describes
the layer and builds tile URLs; the application fetches, caches, decodes and
draws. WMS and WMTS fit unchanged.

### 5.6 Segments (`segments.h`) — stable since ABI 2.0, in place

Given the recorded points of a group of similar activities and the declared
parameter values, return the network (nodes, segments with their tracks,
each track as a chain of segments); cancellable. Two plugins: heat map,
track matching.

`build()` returns 0 with a network, `OR_SEGMENTS_CANCELLED` when it stopped
at the cancel callback's request, or another non-zero value with a message;
`*out` is NULL on every return but success. It refuses malformed input:
a track with points but no coordinates, non-finite or out-of-range
positions, parameter values not finite or outside their declared bounds
(SDK `TracksProblem`, `ParamsProblem`).

**Tracks are recorded data.** Each track is an outing's recorded points as
the input plugin delivered them (time, position, altitude, recorded speed,
heart rate...; a point without position has NaN coordinates). Analysis
works on recorded data, never on thinned tracks or on lines derived from
them, whose features may be artefacts of that derivation; thinned tracks are
for display only. The application loads the points in the network
builder's thread, through the input plugin, and keeps them while their
outings are asked for; outings without any position are left out.

Each route step carries its **point range** in its track's recorded
points (`first`, `last`, inclusive). Consecutive steps share their cut point (a step's first
point is the previous step's last); the first step starts at the track's
first point, the last ends at its last point. A step may cover part of its
segment only (a turn-around within it).

- Plugins fill the ranges once their network is final (after clean-up,
  which merges, drops and splits steps), with the SDK's
  `AssignStepRanges`: each track is cut, in order, into one stretch per
  step of the plugin's chain, minimising the points' distance to their
  step's segment plus each cut's distance to the node between the steps.
  A plugin may fill them from its own matching instead.
- The comparison panel takes its passages from the ranges, on the same
  recorded points (`PassageAlong`); `FindPassages` remains for a network
  without ranges.
- Both in-tree methods work on the recorded points themselves: each
  track's moving points (SDK not-moving rule) with their own velocities
  (SDK `PlaneTrack`, `PointVelocities`). Matching scores points by their
  distance across a segment's line and the path walked; the heat map has
  each point paint the stretch of way it stands for, along its velocity.
- Rests -- a pause at a place, walks around it included (SDK
  `RestIntervals`, from the rolling-window statistics of `RollingStats`) --
  are left out of both methods and break a track into passes; every rest
  place (`RestPlaces`) cuts the segments passing it, and the shared
  clean-up keeps those nodes (`CleanupParams::kept_places`). Both plugins
  declare the rest window and radius as parameters.
- The cancel callback moves to the host services (5.2) in a later minor
  version.

### 5.7 Workflows (`workflow.h`) — planned

Decided in the model: an external executable as backend, its return code
as diagnostic; shown in the status bar with a cancel button; first
workflow: the user's GarminDB download script; rescan after success.
Needs progress and cancellation (5.2).

### 5.8 Metrics (`metrics.h`) — planned

The common ground of statistics, zones, mean-maximal curves, load scores and
user metrics. A metrics plugin declares metrics:

- id, label, unit;
- **scope**: computed per outing, per segment passage, per interval;
- **combination** over several outings or a date range (sum, mean weighted
  by time or distance, max, a curve combined point by point...);
- the settings it reads (e.g. resting/max HR);
- a computation: points (+ settings) -> value(s); a value may be a scalar
  or a curve (e.g. a mean-maximal curve: duration -> best mean).

The application caches results in its own store, keyed by outing and plugin
version, and computes them in the background over the library. Built-in
statistics become the first metrics.

### 5.9 Intervals (`intervals.h`) — planned

Per outing, time ranges with a kind (climb, descent, pause, effort, lap...)
and a label; computed automatically, then editable by the user (decided in
the model: automatic then edited). Interval metrics come from 5.8; matching
intervals across outings uses the segment network and passages.

### 5.10 Export (`export.h`) — planned

Write one or several outings (points, metadata, optionally intervals and
metrics) to a file format (GPX, TCX, FIT...). Sync with online services is an
export (and input) plugin needing the authorisation host service.

### 5.11 Models — not a plugin kind yet

Critical power/speed and W' balance (from mean-maximal metrics), training
load / form / fatigue (from a load metric over time): built into the
application first, over metrics; to become a plugin kind only if
alternatives are worth swapping. Published models are reimplemented from
their papers (Banister; Monod & Scherrer; Skiba); GoldenCheetah's code is
GPL v2 and cannot be taken into OpenRider (BSD-3).

## 6. L1 domains (SDK, C++, namespace `openrider::sdk`)

In place: module `libsdk.m` (static library `openrider_sdk`), headers in
`include/openrider/sdk/`. `libcore` and the in-tree plugins link it; the
plugins' outputs were unchanged by the move (compared byte for byte).

| Domain | Headers | Content | Stability |
|---|---|---|---|
| Geometry | `plane_geometry.h`, `local_projection.h` | local projection; plane points, distances, lengths, point along a line, distance to a line, resampling | stable |
| Rests | `window_stats.h`, `rests.h` | rolling-window statistics of recorded points (extent, displacement, path, speed percentiles, stopped share) and the outing's moving-speed percentiles; rest intervals from criteria a plugin chooses (window, radius, displacement ratio, speed ratio); rest places | provisional |
| Motion | `motion.h`, `velocity.h` | motion parameters; haversine; moving-average speed; percentile; not-moving mask (recorded speed <= 0.3 m/s, positional rule as fallback); velocity of each recorded point | stable |
| Tracks | `recorded_track.h`, `plane_track.h`, `overview.h`, `segment_passages.h`, `route_ranges.h` | recorded track (an outing's recorded points, what analysis works on); its moving points in the plane with velocities and path; thinned track for display (positions, per-point noise, box); thinning; GPS noise per point; position on a line; a passage over a known range; passages found from geometry (fallback); route step ranges | provisional |
| Series | `series.h` | derived series of an outing: speed, altitude, vertical speed and grade over windows, histograms per period; x axes | stable |
| Statistics | `stats.h` | outing statistics from points (moving time from the raw points) | stable |
| Networks | `segments.h`, `network_cleanup.h` | segment network types; clean-up (pass-through merging, short loops, duplicates) | provisional, as 5.6 |
| ABI glue | `segment_exchange.h`, `params.h` | segments kind: C <-> C++ conversions of tracks and networks, the matching `free`, running a plugin, checking the tracks received; checking declared parameter values | stable for the existing kind |
| Result type | `result.h` | `Result<T>` / `Status`, errors without exceptions | stable |

Not moved yet, as they would change the plugins' results or need a design
of their own: the heat map's own geometry helpers (densifying,
simplification), one turn-around detector replacing the two plugins'
variants, and generic declared-parameter helpers (`abi.h`) for all kinds.

Stays private in `libcore`: the outing library and its scan, background
loaders and builders, the activity type catalog, outing selection, plugin
loading, paths, the session log files, long-date formatting
(`core/long_date.h`, split from the overview code).

## 7. Decisions (all taken 2026-10-13)

1. ~~Input threading~~ — decided: each plugin declares whether it is
   thread-safe; if not, the application uses it from one thread only (5.1, 5.3).
2. ~~ABI 2 versioning~~ — accepted: major/minor plus `struct_size` (§4).
3. ~~Host services~~ — accepted: specified now, each implemented with the
   first plugin needing it (5.2).
4. ~~Segments point ranges~~ — decided: per-step point ranges, thinned
   input tracks with original point indices (5.6).

## 8. Next steps (no new feature)

1. ~~Review and decide §7~~ (done).
2. ~~Extract the SDK~~ (done: §6, `plugin_sealing.tst`, outputs identical).
3. ~~ABI 2.0~~ (done): major/minor versions, `struct_size` in every
   function table, the thread-safety flag and single-thread dispatch,
   segment step ranges used by the comparison panel. `init()` and host
   services come in a minor version with the first plugin needing them.
4. Header documentation updated to state each contract above (threading,
   order, identity, stability level).

## 9. Testing a plugin: `openrider-plugin-check`

<!-- @impl 1c06-6ad1-23a6-b98b -->
OpenRider ships `openrider-plugin-check` in `bin/`, beside the `openrider`
executable. It loads a plugin through the application's own loader
(`core::LoadPlugin`, the function the application calls for every library
it finds), so it accepts exactly what the application accepts and nothing
else. **Run it in your plugin's test harness**, with the
`openrider-plugin-check` of each OpenRider version you target: a plugin that
builds and passes its own unit tests can still be refused at load time, and
nothing else tells you. (On Windows, for instance, a DLL silently loses its
entry point when the linker stops exporting everything — 5.1.)

```sh
openrider-plugin-check [--kind KIND] PLUGIN...
```

`KIND` is the plugin's directory name: `input`, `filters`, `tiles`,
`workflows` or `segments`. Without `--kind`, it is the name of the
directory holding `PLUGIN`, so a plugin already in place in
`plugins/<kind>/` needs none.

For each plugin it makes the application's three checks, in order:

1. the library opens (its own run-time dependencies are found);
2. it exports `openrider_plugin` (5.1);
3. its descriptor is accepted for that kind: same ABI major version, a minor
   version no newer than the application's, the kind asked for, a name and
   a function table, and a table at least as large as the kind's ABI 2.0
   table (§4).

It reports in [TAP](https://testanything.org): one test point per check
(after a failure, the later checks of that plugin are skipped), a `#` line
with the reason for each failure, and the accepted plugin's name, version,
ABI and thread-safety. The exit status is the verdict: 0 when every plugin is
accepted, non-zero otherwise (currently 1 when one is refused, 2 on a usage
error; rely only on zero versus non-zero), so any harness can use it as is:

```
$ openrider-plugin-check --kind filters build/libmy_filter.so
# OpenRider plugin ABI 2.0
ok 1 - libmy_filter.so: library opens
ok 2 - libmy_filter.so: exports openrider_plugin
ok 3 - libmy_filter.so: descriptor accepted as a 'filters' plugin
# my_filter 1.0, ABI 2.0, thread-safe
1..3
```

It does not check what the application does not check at load time: run-time
dependency sealing (§2, `plugin_sealing.tst` for in-tree plugins) and the
behaviour of the kind's functions stay with your own tests. OpenRider runs it
on its own plugins (`plugin_check.tst`).
