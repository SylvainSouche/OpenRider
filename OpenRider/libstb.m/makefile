# stb_image (MIT or public domain): fetched from a pinned commit and built
# as a static archive; patches/ adds the one implementation unit (PNG, JPEG).
LIB=stb
IMPORT=fetch:stb
FETCH_URL=https://github.com/nothings/stb/archive/013ac3beddff3dbffafd5177e7972067cd2b5083.tar.gz
FETCH_PATCHES=add-stb-image-impl.patch
SRCS=stb_image_impl.c
IMPORT_HEADERS=stb_image.h
LIB_SHARED=NO
WARN=none
.include "mk.lib.mk"
