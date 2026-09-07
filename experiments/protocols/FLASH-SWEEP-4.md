# FLASHGUARD_FLASH_SWEEP/4

Preserves the `/3` deterministic stimulus families at 5, 7.5, 10, 12, 15, 20, 25, and 30 Hz:

- `luminance_full`
- `red_full`
- `luminance_quarter`

SC 2.3.1 general/red threshold evaluation and calibrated solid-angle branch are preserved.

SC 2.3.2 changes materially. Authoritative transition counting now uses `WCAG_FLASH/3` and B8G8R8A8_UNORM-equivalent replay output. More than six represented light/dark transitions in any one-second window fails: seven transitions are 3.5 flashes. The old internal R16 epsilon reversal count remains in JSON only as a non-normative diagnostic.

The source stimulus must exceed six represented transitions in a one-second window for the SC 2.3.2 branch to be considered exercised.

The regression gate requires the authoritative SC 2.3.1 and SC 2.3.2 results for every declared stimulus.

This is deterministic regression evidence, not external certification or medical-safety validation.
