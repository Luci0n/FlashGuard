# FLASHBENCH/5

Orchestrates build, shader/risk validation, NVOFA smoke, `FLASHGUARD_REPLAY/6`, `FLASHGUARD_FLASH_SWEEP/6`, and summary reporting.

The flash sweep and motion replay now have independent pass/fail status. A motion regression can fail REPLAY/6 without being conflated with flash-evaluator semantics, and a flash regression can fail the sweep independently.

The internal R16 epsilon reversal metric remains informational only.
