# Windows: the application libraries (core, maptiles, app) are static.
# They declare no exports, and a DLL built without any exports everything
# its objects hold, the libstdc++ inline instantiations included. A program
# linking that DLL beside a static library that holds the same
# instantiations (openrider_sdk, reproj) got them twice (app.dll beside
# reproj in perf.tst and in libapp's tests). Plugins stay shared libraries:
# their one export is explicit (OR_PLUGIN_EXPORT), and they never link an
# application library (docs/plugin-api.md §2).
.if empty(PLUGIN_KIND) && !defined(LIB_SHARED)
LIB_SHARED = NO
.endif
