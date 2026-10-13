---
schema_version: 2
id: 0f87-6acd-cd73-d2fa
alias: mvc-views-read-only
type: req
status: confirmed
origin: user statement, 2026-10-13: move external modules/dependencies into an OpenRiderDependancy framework; the app proper moves to a strict airtight MVC pattern; when a view needs an intermediate model of its own (ViewModel) only to maintain the representation of the underlying model, associate it with a ViewController, and tell the user when the case arises
created: 2026-10-13
rel_decided_by: [0f87-6acd-cd73-27ab]
rel_refines: [0f87-6acc-f56f-1211]
generator: model.py/2.4.0
checksum: 9ad872a3b0c5
---

# Views shall only read models and view models (const access) and report user intents; they shall never change state

<!-- rationale, detail, alternatives considered -->
