# WCAG_FLASH/3

## Reference

This profile follows the relevant flash definitions in WCAG 2.2 Success Criteria 2.3.1 and 2.3.2 and sufficient technique G19 for FlashGuard's deterministic synthetic corpus.

## SC 2.3.1 branch

Preserves the existing temporal-extrema general-flash and saturated-red calculations and the calibrated `0.006` steradian area branch.

## SC 2.3.2 / G19 branch

The replay output is quantized per sampled RGB channel to 8-bit UNORM precision before transition counting. Temporal extrema are then determined with exact equality on the quantized trace. Every represented light/dark direction change counts regardless of brightness or area. The maximum number of transitions in any one-second window must be `<= 6`; seven transitions are 3.5 flashes and fail.

The source stimulus must exceed six transitions in a one-second window for this branch to be considered exercised.

## Internal diagnostic

The historical R16 `1e-7` reversal counter is retained under `*_internal_r16_epsilon_*` field names. It is not used for SC 2.3.2 pass/fail.

## Limits

This profile analyzes FlashGuard's declared synthetic corpus, not arbitrary captured video or every possible spatial mask. Passing it is regression evidence, not W3C certification or medical-safety validation.

References: https://www.w3.org/WAI/WCAG22/Understanding/three-flashes ; https://www.w3.org/WAI/WCAG22/Techniques/general/G19
