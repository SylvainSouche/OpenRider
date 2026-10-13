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
   never on an application library. (Today `filter_jitter`,
   `segments_heatmap` and `segments_matching` load `libcore.1.dylib`: this
   is what the SDK fixes.) A test checks the installed plugins' run-time
   dependencies.
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

Today: `OR_PLUGIN_API_VERSION` (1) must equal the application's, exactly.
Any change, even an addition, would refuse every built plugin.

Proposed for ABI 2:

- `api_version` becomes **major** (refusal on mismatch) plus **minor**
  (`api_minor`): a plugin built for minor *n* loads in an application of
  minor >= *n*.
- Every vtable and host table starts with a `size_t struct_size`; functions
  are only ever appended, so a newer application can tell which functions an
  older plugin provides, and the reverse.
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

ABI 2 adds `api_minor`, an optional `init(const or_host *host)` called
once after loading (see 5.2), and `flags`. The first flag,
`OR_PLUGIN_THREAD_SAFE`, says the plugin may be called from several threads
at once; without it the application calls the plugin from one thread only.
This holds for every kind (input, filters, segments...). ABI 1 plugins have
no flags and are treated as not thread-safe until rebuilt; the in-tree
plugins that are thread-safe (GarminDB through SQLite's serialised mode,
the pure filters and segments plugins) declare it.

### 5.2 Host services (`host.h`) — planned, ABI 2

What only the running application can do, offered to plugins as a table of
callbacks handed over by `init()`:

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
  several threads (scan, overview loading, user actions). Decided: the
  plugin **declares** whether it is thread-safe (§5.1, `OR_PLUGIN_THREAD_SAFE`).
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

### 5.6 Segments (`segments.h`) — provisional, in place

Given the tracks of a group of similar activities and the declared
parameter values, return the network (nodes, segments with their tracks,
each track as a chain of segments); cancellable. Two plugins: heat map,
track matching.

Decided before declaring it stable:

- each route step carries its **point range** (`first`, `last`, inclusive)
  in its track, from the plugin's own matching: partial passages
  (turn-arounds within a segment) are kept, and the application no longer
  re-finds passages geometrically with its own tolerances (`FindPassages`
  becomes a fallback or goes);
- input tracks stay **thinned** (overview tracks), and each carries, per
  point, its **index in the outing's original points**
  (`const uint32_t *source_index`), so that ranges map back to the full
  recording;
- the cancel callback moves to the host services (5.2).

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

| Domain | Header | Content | Stability | From today's code |
|---|---|---|---|---|
| Geometry | `sdk/geometry.h` | haversine; local projection; plane points, distances, lengths, point along a line, distance to a line, resampling, densifying, Douglas-Peucker simplification | stable | `core/motion.h` (haversine), `core/local_projection.h`, `core/plane_geometry.h`, the heat map's `geometry.h` |
| Tracks | `sdk/track.h` | a track type (positions, times, per-point noise); thinning; points while stopped; GPS jitter estimate; turn-arounds (one detector, the two in-tree variants merged); passages along a line | provisional | `core/overview.h` (thinning), `core/segment_passages.h`, the two plugins' turn-around detectors |
| Motion | `sdk/motion.h` | motion parameters; moving-average speed; percentile; still window; not-moving mask | stable | `core/motion.h` |
| Series | `sdk/series.h` | derived series of an outing: speed, altitude, vertical speed and grade over windows, histograms per period; x axes | stable | `core/series.h` |
| Statistics | `sdk/stats.h` | outing statistics from points (moving time from the raw points) | stable | `core/stats.h` |
| Networks | `sdk/network.h` | segment network types; clean-up (pass-through merging, short loops, duplicates) | provisional, as 5.6 | `core/segments.h`, `core/network_cleanup.h` |
| ABI glue | `sdk/abi.h` | declared parameters and their default values; writing an error into the caller's buffer; C <-> C++ conversions for each kind (tracks, networks...) and the matching `free` | stable for existing kinds | `core/segment_exchange.h` |
| Result type | `sdk/result.h` | `Result<T>` / `Status`, errors without exceptions | stable | `core/result.h` |

Stays private in `libcore`: the outing library and its scan, background
loaders and builders, the activity type catalog, outing selection, plugin
loading, paths, the session log files, long-date formatting.

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
2. Extract the SDK (§6) and move the in-tree plugins onto it; a test checks
   no plugin depends on an application library; plugin outputs compared
   byte for byte before and after.
3. ABI 2: versioning, `flags` with the thread-safety flag and the
   application's single-thread dispatch for the others, `init()`, host log
   and progress, segment step ranges with source indices (the comparison
   panel uses them); in-tree plugins rebuilt.
4. Header documentation updated to state each contract above (threading,
   order, identity, stability level).
