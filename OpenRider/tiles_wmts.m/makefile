# @impl 0f87-6ada-53d7-4404
# WMTS tiles plugin: layers described by an OGC WMTS service's capabilities
# (tile matrix sets in each coordinate system the provider publishes, REST
# or KVP tile requests). A shared library loaded at run time from
# plugins/tiles/.
LIB=openrider_tiles_wmts
PLUGIN_KIND=tiles
LIBS=cjson
TESTS_CXX=xml_test capabilities_test wmts_test
.include "mk.lib.mk"
