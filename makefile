# OpenRider -- Bmake It workspace (https://github.com/SylvainSouche/bmake-it).
# bmake must find Bmake It's mk/ directory: run its scripts/install-env.sh
# once, or set MAKESYSPATH=<bmake-it>/mk:<default>.
PARENT_WS=
.include "mk.workspace.mk"

# Test data fetched, not stored: the GPX tracks testdata/cc-gpx.tsv lists,
# downloaded once from Common Crawl's archives into testdata/cc-gpx/
# (ignored by git; tracks already there with the right SHA-256 are kept).
# The GPX reader's corpus test uses them when present.
fetch-test-data: .PHONY
	sh ${.CURDIR}/tools/fetch-cc-gpx.sh ${.CURDIR}/testdata/cc-gpx.tsv ${.CURDIR}/testdata/cc-gpx

# The scaling corpus: thousands of GPX tracks testdata/cc-gpx-scale.tsv.gz
# lists, fetched the same way into testdata/cc-gpx-scale/ (several hundred
# MB). scale.tst uses them; without them it is skipped.
fetch-scale-data: .PHONY
	sh ${.CURDIR}/tools/fetch-cc-gpx.sh ${.CURDIR}/testdata/cc-gpx-scale.tsv.gz ${.CURDIR}/testdata/cc-gpx-scale
