# Windows. Under Cygwin (the runtime Bmake It runs the tests with), strict
# -std=c++20 hides the POSIX extensions (usleep() in ImGui's GLFW backend):
# _DEFAULT_SOURCE shows them. Native runtimes (ABI=mingw, MSVC) are not POSIX.
.if !empty(_TARGET_KEY:M*-cygwin)
CFLAGS += -D_DEFAULT_SOURCE
CXXFLAGS += -D_DEFAULT_SOURCE
.endif

# MSVC: windows.h, which curl's header pulls in too, defines min and max as
# macros unless told otherwise, and std::max then does not compile.
.if ${TOOLCHAIN} == "msvc" || ${ABI:U} == "msvc"
CFLAGS += -DNOMINMAX
CXXFLAGS += -DNOMINMAX
# curl is compiled static for this output (libcurl.m/mk/pre.win.mk): its header
# must not declare dllimport.
CFLAGS += -DCURL_STATICLIB
CXXFLAGS += -DCURL_STATICLIB
.endif

# mingw-w64: the compiler's runtime (libgcc, libstdc++, winpthread) is linked
# in, so the programs, the plugins and the tests start without the mingw
# runtime DLLs beside them or on PATH. GNU ld reads archives once, in order:
# winpthread whole, so that libstdc++ and the driver's own -lpthread, later on
# the line, find it already linked, never its DLL. lld searches archives
# whatever their order, and --whole-archive would only add code a DLL exports.
.if ${ABI:U} == "mingw"
LDFLAGS += -static-libgcc -static-libstdc++
.  if ${TOOLCHAIN} == "gcc"
LDFLAGS += -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive,-Bdynamic
.  else
LDFLAGS += -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic
.  endif
.endif
