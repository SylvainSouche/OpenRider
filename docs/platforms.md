# Platforms: build and test matrix

OpenRider is built and its whole test suite run natively on each platform
below (2026-10-20). The Linux platforms run in containers and the BSDs in
QEMU guests, both from the Bmake It cross-platform harness
(`tests/harness` in Bmake It: `vm.sh` for the guests, the `bmk/*` container
images).

## Results

| Platform | clang | gcc |
|---|---|---|
| macOS 15 arm64 (host, MacPorts) | all pass | — |
| Debian 13 arm64 (container) | all pass | all pass |
| Debian 13 amd64 (container, emulated) | all pass | — |
| Alpine 3.24 (musl) amd64 (container, emulated) | all pass | — |
| FreeBSD 15.1 arm64 (QEMU guest) | all pass | builds; tests cannot link (see below) |
| NetBSD 10.2 arm64 (QEMU guest) | all pass | all pass |

"All pass": every module builds -- dependencies, libraries, the five
plugins, the application -- and every test suite passes (SDK, core, app,
map tiles, each plugin, GPX, plugin checks; 12 suites).

Not covered: Windows (built with mingw-w64 elsewhere; no Windows guest),
FreeBSD and NetBSD amd64 (no guest on an arm64 host), and running the GUI
itself on Linux or the BSDs (no display in the containers and guests).

## What each platform needs

Besides Bmake It, `bmake`, a C++20 compiler, `pkg-config`, ATF and Kyua:

| Platform | Packages |
|---|---|
| macOS (MacPorts) | `bmake glfw curl tiff atf kyua pkgconfig` |
| Debian / Ubuntu | `bmake libatf-dev kyua pkgconf libglfw3-dev libcurl4-openssl-dev libtiff-dev libgl-dev` |
| Alpine | `bmake atf-dev kyua pkgconf glfw-dev curl-dev tiff-dev mesa-dev` |
| FreeBSD | base `make` (bmake), clang and atf; packages `kyua pkgconf tiff glfw curl libglvnd` |
| NetBSD | base `make` (bmake), gcc and atf; pkgsrc `pkgconf kyua clang lld tiff curl glfw` |

OpenGL is whatever `libGL` the system provides (Mesa through libglvnd on
Linux and FreeBSD, X11's on NetBSD); to *run* the application, an OpenGL
implementation such as Mesa and a display are needed too.

The other dependencies (cJSON, Dear ImGui, ImPlot, SQLite, stb, Phosphor) are
fetched as sources by the build: the first build needs the network.

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
`TOOLCHAIN=gcc` to bmake for gcc).

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
  headers. Until Bmake It calls `${MAKE}` there, give the guest a `bmake`:
  `ln -s /usr/bin/make /usr/pkg/bin/bmake`.
- **Linux file times** are too coarse to order files written within a few
  milliseconds; the tile cache therefore never evicts the tile it has just
  written (found by the Linux runs).
