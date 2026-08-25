# FLASHGUARD_REPLAY/2

## Change from /1

The deterministic D3D11 replay corpus and motion thresholds are unchanged. The overall replay result now depends on `FLASHGUARD_FLASH_SWEEP/2`, which replaces the earlier adjacent-frame flash counter with temporal-extrema evaluation, uses WCAG 2.2 linear-sRGB general-flash and CIE 1976 u-prime/v-prime saturated-red definitions, evaluates maximum one-second windows, and records calibrated flash-region solid angle.

Old `FLASHGUARD_REPLAY/1` results remain preserved and comparable only under their original protocol.
