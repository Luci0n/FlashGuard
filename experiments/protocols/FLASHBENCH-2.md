# FLASHBENCH/2

## Change from /1

`FLASHBENCH/2` requires the WCAG-oriented flash evaluation defined by `FLASHGUARD_FLASH_SWEEP/2` and `WCAG_FLASH/1`. The build, shader, NVOFA, motion, visual-replay, and artifact-retention layers are otherwise unchanged.

The summary retains the earlier engineering metrics and adds explicit version-2 WCAG sweep status, including separate maximum output general/red flash counts and aggregate SC 2.3.1 / SC 2.3.2 results.

A successful GPU run therefore requires both the existing motion/quality regressions and the version-2 flash evaluation to succeed.
