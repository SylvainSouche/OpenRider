---
schema_version: 2
id: 0f87-6acd-d0f1-eb6d
alias: rule-comment-complex
type: req
status: confirmed
origin: user statement, 2026-10-13: coding rules: classes and methods are CamelCase; each class has its dedicated source file and header; utility functions outside classes go in a module-level header/source; classes are documented (Doxygen or similar supported by Bmake It); all methods documented; comments explain complex methods, plus along the implementation; methods and parameters self-documenting, parameters lowercase, except trivial ones where the state of the art accepts otherwise
created: 2026-10-13
rel_decided_by: [0f87-6acd-d0f1-36e0]
rel_refines: [0f87-6acc-e9e7-920f]
generator: model.py/2.4.0
checksum: 98d6ffcf5236
---

# Complex methods shall be explained in their documentation and by comments along the implementation

<!-- rationale, detail, alternatives considered -->
