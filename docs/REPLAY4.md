# REPLAY/4 protocol

REPLAY/4 is FlashGuard's synthetic motion and temporal measurement protocol for the 0.2 development line. It is an engineering regression protocol, not a medical-safety certification.

## Time and motion

Replay time is virtual. `fps` defines logical `dt`; wall-clock execution speed is irrelevant. Motion reports both requested physical velocity and realized per-frame coordinates in `motion-realization.json`.

The scrolling corpus deliberately contains two distinct cases:

- `smooth_subpixel_scroll` translates a rasterized small-font desktop text surface at fractional-pixel offsets with linear coverage interpolation.
- `integer_snapped_scroll` uses the same requested pixels/second but rounds each frame to an integer coordinate. It intentionally preserves move/stall cadence seen in snapped UI motion.

Results from these cases must not be treated as equivalent stimuli merely because individual frames can share the same displacement.

## Full-resolution motion diagnostics

`MOTION_DIAGNOSTICS/2` replaces the old x-interleaved diagnostic target. Replay binds three full-resolution R16G16B16A16_FLOAT MRTs:

1. global flow, current-surface transport, vacated surface, disocclusion infill;
2. portable matcher, combined hardware motion, effective motion, repeated-risk memory;
3. verified flash override, coarse GPU motion, CPU motion prior, temporal mask.

Every metric exists at every pixel (`sampling_stride=1`). GPU-to-CPU diagnostic collection is temporally capped near 30 samples/second so full spatial coverage does not multiply replay cost at high logical FPS. Reports include whole-frame, active-rectangle, outside-rectangle, and source-changed-pixel aggregates.

## New record-only cases

REPLAY/4 adds measurements for smooth text scrolling, integer-snapped text scrolling, scroll-stop recovery, saturated-red translation, and a moving object that intrinsically flashes. Existing moving-square trailing-edge measurements continue to exercise basic vacated/disoccluded background behavior.

These new measurements are record-only in the initial REPLAY/4 implementation. Existing REPLAY/3 pass thresholds remain unchanged until a REPLAY/4 baseline has been reviewed.

## Recovery metric

After smooth scrolling stops, the source is held at the exact final position. `scroll_stop_recovery_ms` records when source/output MAE remains below 0.003 for three logical frames, bounded by the half-second observation window. A new intrinsic hazard during a future recovery test must abort that interpretation rather than being forced through recovery.

## Flash evaluator boundary

REPLAY/4 is paired with `FLASHGUARD_FLASH_SWEEP/4` / `WCAG_FLASH/3` beginning with the first post-measurement baseline. SC 2.3.2/G19 transition counting is performed after B8G8R8A8_UNORM-equivalent output quantization so sub-LSB R16 feedback noise cannot become a standards failure. The historical R16 `1e-7` reversal count remains available only as a non-normative diagnostic.

This change does not alter REPLAY/4 motion stimuli, motion thresholds, or runtime filtering behavior.
