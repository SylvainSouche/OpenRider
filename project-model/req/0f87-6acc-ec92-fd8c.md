---
schema_version: 2
id: 0f87-6acc-ec92-fd8c
alias: fetch-delay-initial
type: req
status: confirmed
origin: user statement, 2026-10-12: active caching (map displayed): 4 parallel loads with thread/connection pool; delay between loads; on d/l error double the delay + 500ms (no error 100ms, 1 error 700ms, 2 errors 1.5s ...); 10% delay reduction after success, never below 100ms
created: 2026-10-12
rel_decided_by: [0f87-6acc-ec91-4c67]
rel_refines: [0f87-6acc-ec91-4ea6]
generator: model.py/2.4.0
checksum: ae8f284d3912
---

# The delay between loads shall start at 100ms

<!-- rationale, detail, alternatives considered -->
