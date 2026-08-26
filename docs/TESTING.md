# Testing and experimental methodology

FlashGuard uses deterministic GPU regression tests to make behavioral changes measurable and reproducible. Test results are evidence about the implemented corpus and environment; they are not a medical guarantee and are not a substitute for formal photosensitive-epilepsy analysis or clinical validation.

## Test layers

### Build and shader validation

A release MSVC build verifies the Windows application compiles. `FlashGuard.exe --validate-shaders` compiles every embedded HLSL entry point.

### NVOFA smoke test

`NVOF_SMOKE/1` creates a deterministic D3D11 optical-flow pair on a supported NVIDIA GPU and requires a non-zero flow field. The report records output grid, number of non-zero vectors, total vectors, and mean absolute flow magnitude.

### Synthetic replay

`FLASHGUARD_REPLAY/6` feeds generated frames through the actual D3D11 analysis/safety/render path rather than a simplified CPU model. Current coverage includes:

- static gray control
- 15 Hz full-screen dark/bright flash
- straight, oblique, and small bright-object motion
- procedural camera pans at multiple speeds
- smooth fractional-pixel scrolling of rasterized small desktop text
- integer-snapped scrolling with intentional move/stall cadence
- scroll-stop recovery
- saturated-red translation
- a translating object with intrinsic 10 Hz flashing

Reported metrics include static MAE, flash modulation reduction, legacy whole-background ghost MAE, inside-object MAE, edge MAE, pan and scroll MAE, scroll-stop recovery time, saturated-red motion error, moving-flash reduction, full-resolution motion-classification statistics, realized motion coordinates, and NVOFA execution counts.

`FLASHGUARD_TRAIL_METRICS/1` records the exact pixels vacated by a moving synthetic object at full pixel resolution in `trail-metrics.json`. It reports mean residual error, worst-frame p95/p99 and peak residual, the maximum fraction of vacated pixels above 2% and 5% error, and time for the final footprint to clear below 1%, 2%, and 5% p99 error. These metrics are initially calibration evidence rather than hard pass/fail gates. They were added because a thin visible trail can be strongly diluted by whole-frame or whole-background averages.

### Low-contrast perceptual flash sweep

`FLASHGUARD_PERCEPTUAL_SWEEP/1` measures static lower-contrast flashing separately from normative WCAG evaluation. The current calibration grid uses source-code deltas `4, 8, 12, 16, 24, 32`, frequencies `5, 10, 15 Hz`, and two half-frame phase offsets on a mid-dark background. It records modulation reduction and peak output delta. These cases are not WCAG pass/fail claims; they detect protection discontinuities that can still be visually obvious.

### Batched parameter screening

`FLASHGUARD_REPLAY_BATCH/1` keeps one replay window, D3D11 device, shader set, and NVOFA session alive while applying multiple runtime configurations. Temporal/filter state is reset between replay cases. A TSV plan supplies `name`, profile, full-screen sensitivity, small-source sensitivity, FPS, and motion scale.

`FLASHGUARD_MATRIX/3` uses two stages:

1. screen all 27 independent profile/full/small sensitivity combinations at 320x180 and 30 FPS with shorter cases and an 8-pixel general metric stride
2. on full runs, rerun only the screening winner and production default at canonical 640x360 and 60 FPS

`FLASHGUARD_SCREENING_PROBES/3` adds four cheap threshold probes near the configured full-screen and small-source sensitivity boundaries. The full-screen probes disable competing compact/coherent/pattern paths so the global-delta threshold is the discriminating gate; the small-source probes disable broad/global paths. The 3x redundant irrelevant-axis combinations are also a consistency check.

The full-screen probes use an sRGB code-96 background with code 139 (~0.141 linear-luminance delta) and code 148 (~0.179), bracketing the configured 0.12/0.16/0.20 global-delta thresholds. The compact probes use code 168 (~0.275 linear-luminance delta) at 0.6% and 1.1% target area, bracketing the configured 0.4%/0.8%/1.5% small-source area thresholds.

The matrix emits `screening_probe_contract`. Probe data is rankable only when every candidate produced the v3 schema, full-probe values remain invariant across the small-sensitivity axis, small-probe values remain invariant across the full-sensitivity axis, and both probe families show the expected threshold ordering. A failed probe contract keeps the raw diagnostics but removes probe values from selection automatically.

Candidate selection first requires a valid replay, then retains the Pareto frontier across flash attenuation, moving-flash attenuation, vacated-trail p99/peak, small-object trail p99, pan MAE, static MAE, low-contrast perceptual behavior, and contract-valid screening probes when available. A worst relative regression against the production default is used only to choose among Pareto candidates; the old weighted magic score is not used.

If GitHub's self-hosted Actions `acquirejob` endpoint returns HTTP 500 and the runner later reports `MissingKey`, no FlashBench step has executed. Treat that as an infrastructure failure: keep the runner connected and trigger a fresh push/run rather than archiving it as benchmark evidence.

Visual replay is available with `-VisualReplay`; sampled frames are rendered as `SOURCE | FILTERED | 6x AMPLIFIED DIFFERENCE` and indexed by an HTML viewer.

### 5-30 Hz WCAG-oriented flash sweep

`FLASHGUARD_FLASH_SWEEP/6` runs the same 24 deterministic two-second cases at 60 FPS and evaluates them through `WCAG_FLASH/5`.

Frequencies:

```text
5, 7.5, 10, 12, 15, 20, 25, 30 Hz
```

Stimulus families:

```text
luminance_full
red_full
luminance_quarter
```

Each generated square wave uses a 50% phase duty cycle. The evaluator reduces each measured trace to temporal extrema, forms qualifying opposing transitions, and records the maximum completed flash pairs found in any one-second window. This avoids treating a multi-frame filtered transition as safe merely because no single adjacent-frame step crosses the threshold.

General flashes use WCAG 2.2 relative luminance from linearized sRGB: an endpoint-to-endpoint change of at least `0.10` with the darker endpoint below `0.80`. Saturated-red flashes use linear RGB converted through CIE XYZ to CIE 1976 UCS; a transition qualifies when either endpoint has `R/(R+G+B) >= 0.8` and the u-prime/v-prime distance is greater than `0.2`.

The synthetic rectangle's solid angle is calculated from the configured display diagonal and viewing distance. SC 2.3.1 keeps the general/red threshold and `0.006` steradian area branches. Output color is evaluated after B8G8R8A8_UNORM-equivalent quantization.

SC 2.3.2 uses the same WCAG general-flash and saturated-red definitions but does not allow the SC 2.3.1 area exemption. The output must remain at or below three general flashes and three red flashes in every one-second window regardless of flashing area. G19 remains a sufficient technique rather than an additional one-code transition definition. The historical internal R16 `1e-7` reversal count is still reported, but it is explicitly non-normative and cannot fail the suite.

The declared synthetic corpus remains regression evidence rather than a full arbitrary-video conformance analyzer or medical-safety certification.

The current regression pass condition is:

```text
source stimulus > 3 completed flashes in at least one one-second window
filtered general flashes <= 3 in every one-second window
filtered red flashes <= 3 in every one-second window
SC 2.3.2 uses the same <=3 general/red flash limits with no area exemption
```

This is WCAG-oriented regression evidence, not a complete WCAG conformance analyzer or a Harding FPA/PSE certificate. The synthetic area calculation does not scan arbitrary spatial masks within every possible 10-degree visual field, and the corpus still does not cover every waveform, display, or real game sequence.

## Reproducibility

From the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File .\flashbench\run.ps1 `
    -Mode gpu-smoke `
    -OutputDir .\flashbench\manual-results
```

For visual replay:

```powershell
powershell -ExecutionPolicy Bypass -File .\flashbench\run.ps1 `
    -Mode gpu-smoke `
    -OutputDir .\flashbench\manual-results `
    -VisualReplay

Start-Process .\flashbench\manual-results\visual\index.html
```

Important outputs:

```text
summary.json
nvof-smoke.json
synthetic-replay.json
flash-sweep.json
trail-metrics.json
perceptual-sweep.json
matrix/matrix.json
motion-diagnostics.json
motion-realization.json
flashbench.log
```

A reproducible result must be tied to the exact Git commit that generated it. GPU, driver, OS, test FPS/resolution, schema version, and relevant parameters should be retained with the run.

## Experimental record policy

Published experiment directories under `experiments/runs/` are append-only evidence. Do not edit an old result to reflect a newer interpretation or implementation.

If a test definition changes materially:

1. keep the old run and protocol
2. increment the protocol/schema version
3. rerun the experiment under the new protocol
4. compare only like-for-like protocol versions unless the difference is explicitly discussed

Failures are retained. A failure followed by a hypothesis, intervention, and successful rerun is more informative than storing only the final passing result.

## Current archived flash-sweep sequence

Two complete GPU artifact sets are preserved:

- `a50412ae415469a9f5e6beecca3c286a7fcc416e`: failed because saturated-red output still produced 5, 7.5, and 10 completed red flashes/s at those input frequencies.
- `1802a4e68656d432a10ce2bf6ba11060ed8d9788`: after holding red mitigation through flash-risk memory, all 24 cases reported 0.000 counted output general flashes/s and 0.000 counted output red flashes/s.

The raw JSON is stored unchanged in each run directory. Checksums are recorded in `RUN.json`.

## Interpretation rules

- Passing the synthetic suite means the exact tested implementation met the declared regression criteria on the declared environment.
- It does not establish that every possible game, display, refresh rate, viewing geometry, color stimulus, duty cycle, spatial pattern, or human response is safe.
- Perceived gameplay latency is currently partly qualitative; deterministic texture replay does not measure end-to-end Desktop Duplication frame age.
- A result generated after changing a protocol cannot be directly called an improvement over an older protocol without accounting for the protocol difference.
- Thresholds should not be loosened merely to obtain a passing result. If a regression fails, fix the mechanism or document why the metric itself is invalid before changing the criterion.

## Important gaps

High-value future validation includes:

- finer frequency increments and frequencies below 5 Hz
- varied duty cycles
- additional flash-area shapes and calibrated visual-angle geometry
- spatial-pattern stimuli
- real captured gameplay sequences with exact timestamps
- display-refresh-rate variation
- end-to-end capture-to-display latency instrumentation
- independent external validation against established photosensitive-epilepsy analysis tooling

References:

- [WCAG 2.2: Three Flashes or Below Threshold](https://www.w3.org/WAI/WCAG22/Understanding/three-flashes-or-below-threshold)
- [ITU-R BT.1702](https://www.itu.int/rec/R-REC-BT.1702/)
