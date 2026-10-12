# GarminDB input plugin: a shared library loaded at run time from
# plugins/input/. Links SQLite and cJSON statically, so it carries no
# dependency beyond the system's.
LIB=openrider_input_garmindb
PLUGIN_KIND=input
LIBS=sqlite3 cjson
TESTS_CXX=garmindb_test
.include "mk.lib.mk"
