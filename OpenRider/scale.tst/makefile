# Scaling: a library of thousands of real GPX tracks (testdata/cc-gpx-scale,
# fetched by bmake fetch-scale-data from Common Crawl's archives, never
# stored here) scanned, loaded and analysed through the application's own
# classes and plugins; each stage timed against a budget in units of a
# calibration workload, with its memory. Skipped without the tracks.
TEST_PROG=scale_test
LIBS=core
CXXFLAGS+=-DSCALE_DIR=\"${.CURDIR}/../../testdata/cc-gpx-scale\" -DPLUGINS_DIR=\"${.CURDIR}/../../build/${_TARGET_KEY}/plugins\"
.include "mk.tst.mk"
