# WCAG_FLASH/4

## Representation

Standards-facing output measurements use B8G8R8A8_UNORM-equivalent RGB channel codes. Sub-LSB R16 temporal-history variation is excluded from pass/fail and retained only as an engineering diagnostic.

## SC 2.3.1 branch

General-flash and saturated-red output metrics operate on the quantized display representation. The existing temporal-extrema definitions and calibrated `0.006` steradian area branch are preserved for the declared synthetic corpus.

## SC 2.3.2 / G19 branch

Each sampled display pixel is treated as its own temporal component. Its quantized relative-luminance trace is reduced to light/dark direction segments, and the maximum number of represented transitions in any one-second window is calculated. The reported result is the maximum over all sampled pixels.

More than six transitions in a one-second window fails this deliberately strict G19 regression branch; seven transitions correspond to 3.5 flashes.

This replaces `/3`'s frame-average G19 trace, which could reverse when different pixels changed at different times even though no single sampled pixel was flashing.

## Limits

Replay frame metrics currently use a 4-pixel spatial sampling stride. This profile is deterministic regression evidence for FlashGuard's declared synthetic corpus, not a full arbitrary-video WCAG conformance analyzer, W3C certification, or medical-safety validation.

Reference: W3C sufficient technique G19 and WCAG 2.2 Three Flashes.
