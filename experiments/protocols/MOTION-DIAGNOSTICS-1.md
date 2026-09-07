# MOTION_DIAGNOSTICS/1

Purpose: identify which motion-evidence path authorizes temporal-history bypass
without changing the FlashGuard safety evaluator or motion thresholds.

The synthetic replay emits `motion-diagnostics.json` beside its normal replay
report. Diagnostics are observational only; they are not part of replay pass/fail.

## Shader channels

One `R16G16B16A16_FLOAT` replay-only render target is interleaved by pixel X:

- group 0: global flow, current-surface transport, vacated-surface transport,
  conservative disocclusion infill
- group 1: portable image matcher, combined verified hardware motion,
  final effective motion gate, repeated-risk memory
- group 2: verified-flash override, coarse GPU motion, CPU motion prior,
  final temporal mask

Production rendering does not bind this target.

## Cases

The diagnostic report records:

- `quarter_flash_15hz`: the representative SC 2.3.2 failure case
- `moving_square`: cardinal bright-object transport
- `bright_oblique`: non-cardinal bright-object transport
- `small_moving_square`: portable/anchor-only local-motion regression

Each metric includes mean, maximum, fraction above 0.5, and the same statistics
inside and outside the case's active rectangle. Replay samples every fourth pixel;
metric groups remain spatially interleaved so each metric receives approximately
one third of those samples.

## Interpretation

For the quarter-screen flash, any substantial motion activation is a candidate
false bypass. For moving-object cases, compare verified motion against
`repeated_risk`, `verified_flash_override`, and `temporal_mask` to locate sparse
trailing pixels where valid motion evidence loses authority.

Archive the raw JSON unchanged under `experiments/runs/` before making a tuning
decision from it.
