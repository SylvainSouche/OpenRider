---
schema_version: 2
id: 0f87-6acd-ddbe-2e43
alias: ai-resolution-follows-width
type: ai
status: proposed
origin: proposed by Claude, unapproved (implemented, awaiting confirmation)
created: 2026-10-13
rel_resolves: [0f87-6acd-ddbe-1efd]
generator: model.py/2.4.0
checksum: 2042f4aea1fd
---

# Nothing finer than the trace width is resolved: route entry/exit points within max(30 m, width) are one cut, and any segment shorter than the width is contracted into one node (routes stay connected through it). Result on real data: no segment shorter than the width; running at 50 m: 261 nodes, 422 segments (200 shared)

> AI proposal — introduced by Claude, not the user. Not a requirement until a DEC records acceptance.

<!-- rationale, detail, alternatives considered -->
