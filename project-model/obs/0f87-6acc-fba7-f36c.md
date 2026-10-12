---
schema_version: 2
id: 0f87-6acc-fba7-f36c
alias: obs-garmin-sh
type: obs
status: recorded
origin: file read by Claude, 2026-10-12
created: 2026-10-12
rel_raised_by: [0f87-6acc-fba7-8075]
generator: model.py/2.4.0
checksum: 7bbd60674ffb
---

# ~/garmin.sh: options -d (download/import), -g (rebuild activities.gpkg), -a (both, default); download = activate ~/venvs/garmin then 'garmindb_cli.py --download --import --latest --activities'; gpkg = gpsbabel FIT->GPX + jq on details JSON + ogr2ogr; paths are relative (./HealthData) so it must run with cwd = $HOME; progress lines start with '==>'

<!-- rationale, detail, alternatives considered -->
