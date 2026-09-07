# FLASHBENCH/4

Orchestrates build, shader/risk validation, NVOFA smoke, `FLASHGUARD_REPLAY/5`, `FLASHGUARD_FLASH_SWEEP/5`, and summary reporting.

Changes from `/3`:

- consumes the corrected spatial G19 evaluator
- reports REPLAY/5 after the scroll-stimulus aliasing fix
- retains internal R16 epsilon flash counts as informational diagnostics only

`status=SUCCESS` requires all invoked authoritative regression stages to pass. A failing legacy motion threshold remains a real replay failure and is not hidden by the evaluator correction.

This is regression tooling, not certification.
