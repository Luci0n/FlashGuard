# REPLAY/5 protocol

REPLAY/5 supersedes the initial REPLAY/4 baseline attempt after two measurement-definition problems were found before filter development continued.

## Corrected scrolling stimulus

The requested scroll velocity is 45 pixels/second at `motion_scale=1`. At 60 logical FPS this is 0.75 pixels/frame, so `smooth_subpixel_scroll` actually exercises fractional raster translation while `integer_snapped_scroll` preserves the intentionally stepped move/stall sequence.

Both cases retain the same requested physical velocity across replay frame rates. `MOTION_REALIZATION/1` records the realized offsets.

## Corrected flash measurement boundary

REPLAY/5 uses `FLASHGUARD_FLASH_SWEEP/5` and `WCAG_FLASH/4`.

The output is quantized to B8G8R8A8_UNORM-equivalent RGB before standards-facing flash evaluation. SC 2.3.2/G19 transition counts are tracked independently for each sampled display pixel and the maximum one-second rate is reported. Spatially unrelated one-code changes can therefore no longer synthesize a false flash through frame averaging.

SC 2.3.1 output general-flash and saturated-red calculations use the same 8-bit-equivalent output representation. The historical internal-R16 epsilon reversal counter remains diagnostic only.

## Preserved REPLAY/4 measurements

- full-resolution three-MRT motion diagnostics
- changed-pixel-conditioned diagnostic aggregates
- scroll-stop recovery
- saturated-red translation
- moving intrinsic flash
- realized scroll offsets

New motion measurements remain record-only while the first corrected baseline is reviewed. Existing legacy motion thresholds are not relaxed.

This protocol is engineering regression evidence, not medical-safety or WCAG certification.
