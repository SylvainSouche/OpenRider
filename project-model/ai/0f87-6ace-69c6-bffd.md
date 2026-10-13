---
schema_version: 2
id: 0f87-6ace-69c6-bffd
alias: ai-input-thread-safe
type: ai
status: superseded
origin: assistant proposal in docs/plugin-api.md, 2026-10-13
created: 2026-10-13
rel_informed_by: [0f87-6ace-69c5-f408]
generator: model.py/2.4.0
checksum: c6b0c105dc6a
---

# Input plugins shall be thread-safe: the app calls list_outings/load_points concurrently on one handle; points in time order; outing ids stable across runs (docs/plugin-api.md §5.3)

> AI proposal — introduced by Claude, not the user. Not a requirement until a DEC records acceptance.

<!-- rationale, detail, alternatives considered -->
