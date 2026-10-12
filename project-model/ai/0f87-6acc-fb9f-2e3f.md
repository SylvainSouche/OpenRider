---
schema_version: 2
id: 0f87-6acc-fb9f-2e3f
alias: ai-type-from-gc-json
type: ai
status: accepted
origin: proposed by Claude, unapproved
created: 2026-10-12
rel_decided_by: [0f87-6acd-03a6-6993]
rel_informed_by: [0f87-6acc-f1e3-1918, 0f87-6acc-fb9f-4a91]
rel_resolves: [0f87-6acc-eea3-5b7f]
generator: model.py/2.4.0
checksum: 44ea7699c359
---

# Take each activity's type from Garmin Connect's typeKey in the details JSON (part of the GarminDB file tree), mapped to FIT+tag via a Garmin Connect correspondence table; fall back to the DB's sport/sub_sport only when no JSON exists

> AI proposal — introduced by Claude, not the user. Not a requirement until a DEC records acceptance.

<!-- rationale, detail, alternatives considered -->
