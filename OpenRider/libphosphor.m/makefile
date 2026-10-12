# Phosphor icons (MIT): the regular-weight icon font, fetched from the npm
# release and installed into share/openrider/fonts/ (see mk/local.mk).
# Not code: the patch adds an empty unit so the fetch pipeline has a source.
LIB=phosphor
IMPORT=fetch:phosphor
FETCH_URL=https://registry.npmjs.org/@phosphor-icons/web/-/web-2.1.2.tgz
FETCH_PATCHES=add-stub.patch
SRCS=phosphor_stub.c
LIB_SHARED=NO
WARN=none
.include "mk.lib.mk"
