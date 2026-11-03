# Platforms: build and test matrix

OpenRider is built and its whole test suite run natively on each platform
below (2026-10-23), built with Bmake It at its pull request #17. The Linux platforms run in containers and the BSDs in
QEMU guests, both from the Bmake It cross-platform harness
(`tests/harness` in Bmake It: `vm.sh` for the guests, the `bmk/*` container
images).

## Results

| Platform | clang | gcc |
|---|---|---|
| macOS 15 arm64 (host, MacPorts) | all pass (200 tests) | — |
| Debian 13 arm64 (container) | all pass (200) | all pass (200) |
| Debian 13 amd64 (container, emulated) | all pass (200) | — |
| Alpine 3.24 (musl) amd64 (container, emulated) | all pass (200) | — |
| FreeBSD 15.1 arm64 (QEMU guest) | all pass (200) | builds; tests cannot link (see below) |
| NetBSD 10.2 arm64 (QEMU guest) | all pass (200) | all pass (200) |
| Windows 10 amd64 (host, Cygwin runtime; tests only, see below) | all pass (211, 2026-11-03; GUI test skipped) | all pass (211, 2026-11-03; GUI test skipped) |
| Windows 10 amd64 (host, mingw-w64 runtime) | all pass (211, 2026-11-03; GUI test included) | all pass (211, 2026-11-03; GUI test included) |
| Windows 10 amd64 (host, MSVC output from the EWDK) | clang-cl, `TOOLCHAIN=llvm ABI=msvc`: all pass (211, 2026-11-03; GUI test included) | `cl.exe`, `TOOLCHAIN=msvc`: all pass (211, 2026-11-03; GUI test included) |

"All pass": every module builds -- dependencies, libraries, the plugins,
the application -- and every test suite passes: SDK, core, app, map tiles,
reprojection, each plugin, plugin checks, and the performance suite
(`perf.tst`: budgets in units of a calibration run on the same machine, so
they hold under emulation too); 15 suites, 200 tests.

On Windows, Kyua runs under Cygwin. The Cygwin runtime is a test target only:
its `openrider.exe` needs `cygwin1.dll` and is not what users get, and no
GUI test skips there. The application users get is the native mingw-w64 build
(`bmake ABI=mingw`); with Bmake It d9e3966 and later its tests run as native
programs too (Bmake It builds ATF itself). MSVC output is native too: its
`openrider.exe` imports Windows system DLLs only, and its TIFF decoding is
narrower (Known limits). `gui.tst` (`openrider --frames 120`)
opens the real window for a few seconds wherever a build has a GUI and a
display: it shows up on the screen of whoever runs `bmake test`, with the user
directories redirected to the test's own. The Windows runs had no fetched test
data, so `gpx_test:published_tracks` and the three `scale_test` cases skipped.
The other platforms above were last run before `gui.tst` and the Windows port's
source changes.

Not covered: FreeBSD and NetBSD amd64 (no guest on an arm64 host), and running
the GUI itself on Linux or the BSDs (no display in the containers and guests).
What needs a real machine is listed in [`todo.md`](../todo.md).

## What each platform needs

Besides Bmake It, `bmake`, a C++20 compiler, `pkg-config`, ATF and Kyua:

| Platform | Packages |
|---|---|
| macOS (MacPorts) | `bmake glfw curl tiff atf kyua pkgconfig` |
| Debian / Ubuntu | `bmake libatf-dev kyua pkgconf libglfw3-dev libcurl4-openssl-dev libtiff-dev libgl-dev` |
| Alpine | `bmake atf-dev kyua pkgconf glfw-dev curl-dev tiff-dev mesa-dev` |
| FreeBSD | base `make` (bmake), clang, atf and kyua; packages `pkgconf tiff glfw curl libglvnd` |
| NetBSD | base `make` (bmake), gcc and atf; pkgsrc `pkgconf kyua clang lld tiff curl glfw` |

OpenGL is whatever `libGL` the system provides (Mesa through libglvnd on
Linux and FreeBSD, X11's on NetBSD); to *run* the application, an OpenGL
implementation such as Mesa and a display are needed too.

The other dependencies (cJSON, Dear ImGui, ImPlot, SQLite, stb, Phosphor) are
fetched as sources by the build: the first build needs the network. For MSVC
output, which no package serves, so are curl and libtiff.

## Per-platform build settings

The workspace's `mk/` holds Bmake It hooks read for one target only:

- `mk/pre.freebsd.mk`: `/usr/local/include` (ports' headers: GLFW's header
  includes `<GL/gl.h>` from there) and `-pthread`.
- `mk/pre.netbsd.mk`: `/usr/X11R7/include` (X11's OpenGL headers),
  `-pthread`, and run paths to `/usr/pkg/lib` and `/usr/X11R7/lib`, which
  NetBSD's loader does not search.

Header directories are added with `-idirafter`: searched after every other
directory, so the headers the imports staged always come first. On NetBSD
`/usr/pkg/include` is deliberately not added: a directory named twice keeps
its first place, and pkgsrc packages (ATF) add it with `-I`, ahead of the
base system's copies.

- `mk/pre.win.mk`: under Cygwin, `-D_DEFAULT_SOURCE` (strict `-std=c++20`
  hides `usleep()`, used by ImGui's GLFW backend); for mingw-w64, the compiler
  runtime (libgcc, libstdc++, winpthread) linked statically; for MSVC output,
  `NOMINMAX` and `CURL_STATICLIB`.
- `mk/pre.llvm.mk`: its forced `-include` is not for clang-cl, which takes no
  such option.
- `OpenRider/mk/pre.mk`: `TEST_LINK=objects` on plugins, whose tests reach
  internals the plugin does not export.
- `OpenRider/mk/pre.win.mk`: the application libraries (core, maptiles, app)
  are static on Windows. They declare no exports, and as DLLs they exported
  every libstdc++ inline instantiation, which a program linking them beside
  `openrider_sdk` or `reproj` then had twice. Plugins stay DLLs.

Paths compiled into a test use `${NATIVE_CURDIR}`, which a native Windows program
can open (`C:/...`); elsewhere it is `${.CURDIR}`.
- `OpenRiderDependencies/libglfw.m/mk/pre.win.mk`: GLFW is fetched and
  compiled with its Win32 backend; under Cygwin, which does not define
  `_WIN32`, with GLFW's POSIX thread, timer and module sources.
- `OpenRiderDependencies/libcurl.m/mk/pre.win.mk`: for MSVC output, curl is
  fetched and compiled static, with Schannel for TLS and only HTTP(S) and file:.
  `patches/msvc-config.patch` adds the `curl_config.h` its configure step would
  write, since curl's own `config-win32.h` stops on current MSVC. `LIBS=` hands
  the Windows libraries it needs to whatever links it.
- `OpenRiderDependencies/libtiff.m/mk/pre.win.mk`: for MSVC output, libtiff is
  fetched and compiled static with its built-in codecs;
  `patches/msvc-config.patch` adds its two generated headers.

`openrider.m` links OpenGL itself outside macOS (`pkg-config --libs gl`, else
`-lGL`): GLFW's pkg-config does not list it there.

## Reproducing

Linux, in a container (Colima shares `$HOME`, so stage the sources there):

```sh
rsync -a --exclude build/ --exclude .git <GarminViewer>/ ~/stage/GarminViewer/
rsync -a <bmake-it>/mk <bmake-it>/scripts ~/stage/bmake-it/
docker run --rm -v ~/stage:/work -w /work/GarminViewer <image> sh -c \
  'export MAKESYSPATH=/work/bmake-it/mk:/usr/share/mk; bmake && bmake test'
```

where `<image>` is a `bmk/linux-*` image with the Debian or Alpine packages
above added (`--platform linux/amd64` for the emulated ones; pass
`TOOLCHAIN=gcc` to bmake for gcc; clang is Bmake It's default toolchain,
named `llvm`, on every platform).

FreeBSD or NetBSD, in a Bmake It harness guest (one at a time, with the
container VM stopped):

```sh
H=<bmake-it>/tests/harness
sh $H/vm.sh up freebsd_arm64
tar -C ~/stage -cf - GarminViewer bmake-it | sh $H/vm.sh ssh freebsd_arm64 \
  'mkdir -p /root/work && tar -C /root/work -xf -'
sh $H/vm.sh ssh freebsd_arm64 'cd /root/work/GarminViewer &&
  PATH=$PATH:/usr/local/bin:/usr/pkg/bin MAKESYSPATH=/root/work/bmake-it/mk:/usr/share/mk make && make test'
sh $H/vm.sh down freebsd_arm64     # keeps the installed packages; `reset` drops them
```

## Known limits

- **FreeBSD with gcc**: OpenRider builds, but its C++ tests cannot link: the
  base system's `atf-c++` is built with clang against libc++, gcc uses
  libstdc++. Bmake It's own matrix meets the same.
- **NetBSD and Bmake It**: Bmake It reads a framework's `PREREQS` by running a
  command named `bmake`; NetBSD installs bmake as `make` only, so the
  prerequisites read empty and the build cannot find the dependencies'
  headers. Fixed in Bmake It's pull request #17 (it runs `${MAKE}`): the
  NetBSD cells above ran with it and no `bmake` command at all. With an older
  Bmake It, give the guest a `bmake`: `ln -s /usr/bin/make /usr/pkg/bin/bmake`
  (lost whenever the guest's run overlay is recreated).
- **TIFF on MSVC output**: libtiff is built without zlib or libjpeg, so a
  Deflate or JPEG TIFF tile does not decode there. The packaged libtiff on other
  platforms normally reads them. The test suite writes and reads an uncompressed
  TIFF only.
- **No exceptions, no RTTI on MSVC output**: `cl.exe` and clang-cl warn about
  `-fno-exceptions` and `-fno-rtti` and ignore them, so the compiler does not
  enforce the rule there.
- **Cygwin plugin DLLs export everything**: `OR_PLUGIN_EXPORT` is no `dllexport`
  there. The native builds (mingw-w64 and MSVC output) export `openrider_plugin`
  only, so `openrider-plugin-check` catches a missing export on those.
- **Native Windows tests have no POSIX shell**: a test's `system()` goes through
  `cmd.exe`, so the tests use `std::filesystem` and `testing::TempDir()`
  (`testing/temp_dir.h`), not `mkdir -p`, `find` or `mkdtemp`.
- **Running the mingw-w64 application outside `bmake`**: the compiler runtime is
  linked in, but `libcurl` and `libtiff` DLLs must be beside `openrider.exe`
  or on `PATH`; `bmake run` adds them. No package gathers them yet.

## Design notes

- **Tile cache order**: the cache evicts the least recently used tiles by
  file time. File times are read to the nanosecond, but some file systems
  (Linux's coarse timestamps, some container mounts) give files written
  within a few milliseconds the same time; their relative order is then
  arbitrary, which does not matter for eviction. The one guarantee that
  matters is kept explicitly: the tile just written is never among the
  victims (`TileCache::Evict(keep)`, found by the Linux runs).
- **User files**: macOS keeps them in `~/Library` (Application Support,
  Caches, Logs), Windows in `%APPDATA%` and `%LOCALAPPDATA%`, Linux and the
  BSDs in the XDG base directories: `$XDG_DATA_HOME/OpenRider` (default
  `~/.local/share/OpenRider`) for layers, `crs.txt`, GPX files and plugins,
  `$XDG_CACHE_HOME/OpenRider` (`~/.cache/OpenRider`) for tiles,
  `$XDG_STATE_HOME/OpenRider/logs` (`~/.local/state/OpenRider/logs`) for logs.
