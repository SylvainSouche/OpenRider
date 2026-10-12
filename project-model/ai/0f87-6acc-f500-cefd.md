---
schema_version: 2
id: 0f87-6acc-f500-cefd
alias: ai-testable-layering
type: ai
status: accepted
origin: proposed by Claude, unapproved
created: 2026-10-12
rel_decided_by: [0f87-6acc-f56f-b3be]
rel_informed_by: [0f87-6acc-f500-82de, 0f87-6acc-f500-d528]
generator: model.py/2.4.0
checksum: eb5213cfa2a8
---

# Keep all logic (backend, filters, segmentation, metrics, tile cache, i18n) in libraries with no ImGui/GL dependency, tested with ATF via bmake test; the GUI module is a thin layer over them

> AI proposal — introduced by Claude, not the user. Not a requirement until a DEC records acceptance.

<!-- rationale, detail, alternatives considered -->
