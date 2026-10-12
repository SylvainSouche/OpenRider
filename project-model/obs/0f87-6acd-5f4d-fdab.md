---
schema_version: 2
id: 0f87-6acd-5f4d-fdab
alias: obs-moving-time-bigger-filtered
type: obs
status: recorded
origin: analysis by Claude of the user's question 'how can moving time be bigger filtering than without filtering?', 2026-10-13
created: 2026-10-13
rel_raised_by: [0f87-6acd-208a-c12d]
generator: model.py/2.4.0
checksum: d7944c43e47b
---

# Two causes: (1) without a filter the panel shows Garmin's moving time (default source), with a filter OpenRider's computed one, which is always higher (e.g. 3:09:32 vs 3:30:35 on 24431554471) since our rule counts only immobility; (2) computed on filtered points, our own moving time is not stable (1:46:57 raw -> 1:48:15 filtered on 24675082835): a collapsed pause leaves no points to judge immobility, and anchoring the 10th-percentile speed on raw points does not fix it

<!-- rationale, detail, alternatives considered -->
