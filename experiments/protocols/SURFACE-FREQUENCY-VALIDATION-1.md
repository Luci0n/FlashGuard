# SURFACE_FREQUENCY_VALIDATION/1

Validation contract for the current-frame-only surface-frequency backend, executed by
`flashbench/surface-frequency.ps1` against a `surface`-mode build.

The runner retains every report and exit code and fails if a required check fails. It never opens a
visible flashing demonstration.

## Steps

Each step records an exit code and elapsed milliseconds:

```text
shaders          optimized surface build and every embedded shader entry point
risk-integrator  deterministic 30/60/120/240 Hz risk-integrator invariance check
focused          focused GPU cases and expanded regressions
canonical        canonical 640x360 60 FPS replay, flash sweep, perceptual sweep, trail metrics
1080p            offscreen 1920x1080 GPU timing benchmark
```

## Focused cases

Focused stimuli cover weak and full-range static flashes, continuously translating strong and weak
flashes, ordinary moving content, weak out-of-band controls, and extra idle redraws for static and
moving objects, at 60, 120, and 144 captured FPS. In-band cases cover 5, 7.5, 10, 15, 20, and 30 Hz.
Out-of-band controls cover 2 and 4 Hz, and 40 Hz only when the capture rate can sample it. Moving
objects follow a triangle trajectory at 240 pixels/second.

Attenuation is measured in linear luminance after a 0.5-second warmup. A case passes when reduction
exceeds 70% **and** at least one visible high-phase sample carries a correct confirmed frequency.
Reports also give confirmed and observed sample counts: passing does not imply continuous
confirmation or measured first-event protection.

## Expanded regressions

The regression set reads actual GPU output in every RGB channel and every pixel. It covers stable
textures, one-code dither, a pan that stops, white/red flashes, changing colored phases, textured
flashes, moving white/red flashes, approximately equal-luminance red/green flashes, random grain,
motion into colored backgrounds, and stationary regions beside animation.

Measurement rules:

- Moving-object variation is measured in object coordinates; background contamination is measured
  separately. Screen-coordinate moving measurements from earlier reports are not comparable.
- Static and dither checks require less than one encoded code of unwanted output change.
- The stopped-pan check requires that bound after settling. It does not assert that a nonflashing
  pan is unaffected while moving or during the initial release interval.
- Random-grain checks bound added change relative to the source range. This establishes an
  amplification bound, not zero added grain.

## Reported fields

`validation.json` records `passed`, `exe_sha256`, `focused_case_count`, `regression_case_count`,
`failed_original_cases`, `failed_regression_cases`, `flash_sweep_failed_cases`,
`perceptual_minimum_reduction`, `moving_flash_reduction`, `replay_exit_code`,
`benchmark_exit_code`, `live_outlast_verified`, and `gpu_ms` percentiles.

`gpu_ms` covers features, tracking, composite, and total GPU passes measured with nonblocking
timestamp and disjoint queries. Copy, capture, queueing, presentation, and physical display delay
are excluded. These are GPU processing measurements, not end-to-end latency, and not evidence of
performance under a loaded game.

## Interpretation

Thresholds are not relaxed to obtain a pass. A run whose only failure is a known case is recorded
as `passed: false` with the failing case retained.

This is deterministic engineering regression evidence, not external certification, and not a
medical-safety guarantee.
