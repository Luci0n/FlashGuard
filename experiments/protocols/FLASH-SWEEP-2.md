# FLASHGUARD_FLASH_SWEEP/2

## Purpose

Run the existing 24 deterministic 5-30 Hz synthetic cases while evaluating their output under `WCAG_FLASH/1` semantics.

## Environment

- replay resolution: 640x360
- frame rate: 60 FPS
- duration: 120 frames / 2.00 s per case
- square-wave phase duty cycle: 50%
- default replay calibration: 27 inch diagonal, 70 cm viewing distance

## Stimuli

The `/1` stimulus families and frequencies are preserved unchanged: `luminance_full`, `red_full`, and `luminance_quarter` at 5, 7.5, 10, 12, 15, 20, 25, and 30 Hz.

## Evaluation

The evaluator first reduces each measured time series to temporal extrema so a qualifying transition may accumulate across several rendered frames instead of needing to exceed its threshold in one adjacent-frame step. It then forms qualifying opposing transitions and records the maximum number of completed flash pairs found in any one-second window of the two-second trace.

General transitions use WCAG 2.2 relative luminance from linearized sRGB. Saturated-red transitions use the definition in `WCAG_FLASH/1`: either endpoint has `R/(R+G+B) >= 0.8` and their CIE 1976 UCS u-prime/v-prime distance is greater than `0.2`.

For each stimulus the harness also records the solid angle of the centered flashing rectangle from the configured physical display diagonal and viewing distance. SC 2.3.1 is reported as passing when the output stays at or below three general and red flashes in every one-second window, or when the calibrated flashing rectangle is at or below `0.006` steradians. SC 2.3.2 is reported using the stricter no-more-than-three-flashes branch without the area exemption.

The FlashGuard regression gate intentionally requires the SC 2.3.2-style result as well as the SC 2.3.1 result for every stimulus, so the area exemption cannot hide a >3 flashes/s output regression.

## Scope

This is an executable deterministic regression over the declared corpus. The rectangular solid-angle calculation does not scan arbitrary spatial masks inside every possible 10-degree visual field, and the protocol does not implement the fine balanced-pattern exception or arbitrary-video segmentation. It is not an external WCAG or Harding certification.
