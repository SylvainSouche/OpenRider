# GPX input plugin: a shared library loaded at run time from plugins/input/.
# Reads the .gpx files of a folder (and its subfolders), one outing each,
# next to GarminDB.
LIB=openrider_input_gpx
PLUGIN_KIND=input
LIBS=openrider_sdk cjson
TESTS_CXX=gpx_test
# The corpus test reads the tracks bmake fetch-test-data downloads.
CXXFLAGS+=-DGPX_CORPUS_MANIFEST=\"${.CURDIR}/../../testdata/cc-gpx.tsv\" -DGPX_CORPUS_DIR=\"${.CURDIR}/../../testdata/cc-gpx\"
.include "mk.lib.mk"
