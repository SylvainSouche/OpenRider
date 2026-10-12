---
schema_version: 2
id: 0f87-6acd-5c0a-afbb
alias: obs-bmk-copyup-sigkill
type: obs
status: recorded
origin: diagnosed by Claude, 2026-10-13
created: 2026-10-13
rel_raised_by: [0f87-6acc-e46d-af89]
generator: model.py/2.4.0
checksum: 3bacc8209ec1
---

# Bmake It scripts/diff-aware-copy.sh copies up with 'cp -a' onto the existing file (same inode); on macOS arm64 a rebuilt dylib copied over a previously mapped one is SIGKILLed (exit 137, stale code signature): every test program linking the changed libcore was killed. Workaround: delete copied dylibs before rebuilding; fix: copy to a temp file then mv. OpenRider's own plugin install now does copy+rename

<!-- rationale, detail, alternatives considered -->
