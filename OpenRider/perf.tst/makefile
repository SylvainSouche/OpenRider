# Performance of the hot paths (reprojection, track drawing, tile warping):
# each measured against a budget in units of a calibration workload timed
# on the same machine, so the budgets hold on any host, emulated or not.
# Reports every figure; fails only on a gross regression.
# @impl 0f87-6ada-5814-8a3f
TEST_PROG=perf_test
LIBS=app core maptiles reproj
CXXFLAGS+=-DPERF_CRS_FILE=\"${.CURDIR}/../share/common/openrider/crs.txt\"
.include "mk.tst.mk"
