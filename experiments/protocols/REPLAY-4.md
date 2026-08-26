# FLASHGUARD_REPLAY/4

## Purpose

Deterministic D3D11 replay protocol for motion, recovery, intrinsic-flash, and temporal behavior in the 0.2 development line.

## Core additions from /3

- full-resolution three-MRT motion diagnostics via `MOTION_DIAGNOSTICS/2`
- smooth subpixel text scroll and integer-snapped scroll as separate stimuli
- actual realized scroll offsets in `MOTION_REALIZATION/1`
- scroll-stop recovery, saturated-red translation, and moving intrinsic flash

New REPLAY/4 measurements are initially record-only; legacy replay pass gates are retained for the first baseline. Flash pass/fail is delegated to `FLASHGUARD_FLASH_SWEEP/4` / `WCAG_FLASH/3`.
