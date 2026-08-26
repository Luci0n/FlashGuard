# FLASHBENCH/3

## Purpose

Orchestrate build, shader/risk validation, NVOFA smoke, `FLASHGUARD_REPLAY/4`, `FLASHGUARD_FLASH_SWEEP/4`, and summary reporting.

## Changes from /2

- surfaces REPLAY/4 scroll, recovery, red-motion, and moving-flash metrics
- records the replay report status even when replay fails
- summarizes authoritative G19 display-state flash rate separately from the legacy internal R16 epsilon diagnostic
- uses `WCAG_FLASH/3`

`status=SUCCESS` requires all invoked authoritative stages to pass. Internal R16 epsilon counts are informational only.
