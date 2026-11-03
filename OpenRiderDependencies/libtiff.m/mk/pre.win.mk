# MSVC output (cl.exe, clang-cl): no package provides libtiff for it, so fetch
# the release and compile it here. Its two generated headers, written for this
# output, come from patches/msvc-config.patch. Only the codecs that need no other
# library: no Deflate, no JPEG. Overrides the makefile's pkg-config import.
.if ${TOOLCHAIN} == "msvc" || ${ABI:U} == "msvc"
IMPORT=fetch:tiff
REQUIRES=
FETCH_URL=https://download.osgeo.org/libtiff/tiff-4.7.2.tar.gz
FETCH_PATCHES=msvc-config.patch
IMPORT_HEADERS=libtiff/tiffio.h libtiff/tiff.h libtiff/tiffconf.h libtiff/tiffvers.h
SRCS=libtiff/tif_aux.c libtiff/tif_close.c libtiff/tif_codec.c \
     libtiff/tif_color.c libtiff/tif_compress.c libtiff/tif_dir.c \
     libtiff/tif_dirinfo.c libtiff/tif_dirread.c libtiff/tif_dirwrite.c \
     libtiff/tif_dumpmode.c libtiff/tif_error.c libtiff/tif_extension.c \
     libtiff/tif_fax3.c libtiff/tif_fax3sm.c libtiff/tif_flush.c \
     libtiff/tif_getimage.c libtiff/tif_hash_set.c libtiff/tif_luv.c \
     libtiff/tif_lzw.c libtiff/tif_next.c libtiff/tif_open.c \
     libtiff/tif_packbits.c libtiff/tif_predict.c libtiff/tif_print.c \
     libtiff/tif_read.c libtiff/tif_strip.c libtiff/tif_swab.c \
     libtiff/tif_thunder.c libtiff/tif_tile.c libtiff/tif_version.c \
     libtiff/tif_warning.c libtiff/tif_win32.c libtiff/tif_write.c
LIB_SHARED=NO
WARN=none
CFLAGS += -I${.CURDIR}/work/_resolved/libtiff -D_CRT_SECURE_NO_WARNINGS
.endif
