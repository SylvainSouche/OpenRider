# OpenRider plugin SDK: the computations shared by the application and the
# plugins (geometry, tracks, motion, series, statistics, segment networks,
# exchange with the plugin ABI). Static and pure -- no global state,
# threads or I/O -- so that a plugin links it in and depends at run time on
# no application library. Public headers: include/openrider/sdk/.
# @impl 0f87-6ace-6e83-98f6
LIB=openrider_sdk
LIB_SHARED=NO
TESTS_CXX=motion_test series_test network_cleanup_test segment_passages_test route_ranges_test velocity_test
.include "mk.lib.mk"
