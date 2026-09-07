# Surface-frequency experimental build

This is the first implementation of the core GPU architecture proposed in
[the solution review](SOLUTION-2026-09-05.md). It is an experimental alternative,
not a claim that arbitrary imagery now has zero trails or that the full design
is complete. The existing local Matrix 34–45 changes are preserved.

## Status

The current version is **`0.4.0-alpha.4`**. As of `0.4.0-alpha.1` the architecture
passes its complete validation for the first time: **260 of 260 checks**, including
the moving white/red case at 5 Hz and 144 captured FPS that every `0.3.0` candidate
failed. That case now reaches 74.17% against the unchanged 70% requirement, and the
threshold was never relaxed to get there.

This is still experimental software. Passing the suite is deterministic engineering
evidence on one GPU, not a claim that arbitrary imagery has zero trails, that the
full design in the solution review is complete, or that seizures are prevented.

### Correction history

Each row is an archived run under `experiments/runs/`; each `RUN.json` carries the
executable and source hashes that identify it.

| Version | Correction | Result |
| --- | --- | --- |
| `0.3.0-alpha.1` | First current-frame-only detector and compositor | Passed its own runner: 119/119 focused, 78.52% perceptual minimum |
| `0.3.0-alpha.2` | [Noise/color](releases/NOISE-COLOR-CANDIDATE.md): reject one-code dither, stop history inventing events, keep envelopes through color changes | Failed: moving white/red under the 70% gate |
| `0.3.0-alpha.3` | [Local noise](releases/LOCAL-NOISE-CANDIDATE.md): require a local source change, so animation elsewhere cannot fire events in a held region | Failed on the same case; stationary-region error 33.1964 → 0.00005 codes |
| — | [Outlast diagnosis](OUTLAST-NOISE-DIAGNOSIS.md): live probe isolating detector-introduced speckles | Unresolved; two rules reverted, no build released |
| `0.3.0-alpha.4` | [Source-amplitude bound](releases/AMPLITUDE-BOUND-CANDIDATE.md): bound correction by measured source amplitude | Failed on the same case at 66.566%; grain bounded to 3.986 codes |
| `0.4.0-alpha.1` | [HSV correction](releases/HSV-CORRECTION.md): per-channel phase floor replacing the grayscale projection | **Passed: 260/260** |
| `0.4.0-alpha.2` … `alpha.4` | [Overlay UI](releases/OVERLAY-UI.md), [monospace/opacity](releases/MONOSPACE-OPACITY.md), [compact menu and shader cache](releases/COMPACT-STARTUP.md) | Interface and startup only; protection HLSL unchanged by hash |

The corrections are cumulative: the local source-change requirement, the
source-amplitude bound, and the per-channel phase floor are all active in the
current build. The rejected probes listed under *Known limits* stay rejected.

### Still open

- The live Outlast Trials speckles have **not** been revalidated since the
  diagnosis run. Synthetic passes do not establish that the live artifact is gone.
- Provisional attenuation can still affect ordinary color changes; the low-frequency
  provisional hold spans two 5 Hz periods and can prolong attenuation after an
  ordinary change.
- Random-grain checks establish an amplification bound, not zero added grain.

## Run and compare

Build an executable that defaults to this path, then run it:

```powershell
.\scripts\build.ps1 -Mode surface
.\FlashGuard.exe
```

`.\FlashGuard.exe --legacy` selects the previous implementation, so the two paths
can be compared from one build. A normal `release` build defaults to the legacy
path instead and accepts `--surface-frequency`. Run only one instance at a time.

While it runs, F9 displays the active backend, latest completed GPU pass time,
desktop image age, and presentation counters. F10 opens the settings menu and
Escape closes it. F8 holds the manual shield. Ctrl+Shift+F12 exits. GPU
measurements are written on exit to
`%LOCALAPPDATA%\OutlastFlashGuard\surface-frequency-last-run.json`.

The detector uses fixed 5–30 Hz parameters. Static contrast and hotkeys remain
configurable; legacy detector sensitivity and profile settings do not tune it.

## What changed

- GPU features at half source resolution contain linear RGB and a multiscale
  ternary Census descriptor. A bounded structural matcher estimates current-to-
  previous correspondence without NVOFA.
- Each surface cell transports discrete phase timestamps, a period estimate,
  phase extrema, protection lifetime, and motion. State uses integer loads;
  it is never bilinearly blended across owners.
- Complete same-polarity periods establish 5–30 Hz evidence. Original linear
  color changes supply flash amplitude and independent chroma evidence.
- Real capture timestamps drive the live detector. Pointer-only updates and
  idle redraws do not create flash observations. Idle elapsed time is subtracted
  from the next capture interval to avoid counting it twice. Gaps over 250 ms
  reset continuity.
- The full-resolution compositor selects a current owner using color agreement
  and compresses the upper phase toward its lower envelope. It does not read
  previous displayed RGB, spatially warp the image, dilate old risk masks, or
  brighten a dark background left behind by an object.
- Strong first transitions get provisional attenuation before frequency is
  confirmed. A bounded dormant hypothesis helps when the dark phase becomes
  indistinguishable from the background.
- Live and replay invoke the same pipeline. Replay renders into its existing
  readback sink, which is not used as detector/display history. Its old NVOFA
  diagnostic planes do not describe the new matcher. Replay labels the active
  pipeline explicitly, and the legacy NVOFA-execution gate applies only to the
  legacy path; all original image-quality thresholds remain in place.
- Nonblocking timestamp/disjoint queries measure features, tracking, composite,
  and their total. Copy, capture, queueing, presentation, and physical display
  delay are excluded. These are GPU processing measurements, not end-to-end
  latency measurements or evidence of performance under a loaded game.

## Validation on the RTX 3060

The packaged binary is checked by `flashbench/surface-frequency.ps1`, defined by
[`SURFACE_FREQUENCY_VALIDATION/1`](../experiments/protocols/SURFACE-FREQUENCY-VALIDATION-1.md).
Every archived run includes the exact executable hash and hashes of the source
snapshot; the base commit alone does not identify these builds.

### Current result — `0.4.0-alpha.1` and later

Extended by
[`HSV_CORRECTION_VALIDATION/1`](../experiments/protocols/HSV-CORRECTION-VALIDATION-1.md).
Archived [raw results](../experiments/runs/2026-09-07_hsv-correction/).

| Check | Result |
| --- | --- |
| Total checks | 260/260 pass |
| HSV cases (5/10/20/30 Hz at 60/120/144 FPS) | 60/60 pass |
| Minimum HSV RGB variation reduction, settled | 99.24% |
| Minimum HSV RGB variation reduction, early | 99.38% |
| Moving white/red, 5 Hz at 144 FPS | 74.17% (requirement 70%) |
| 1080p GPU draw time | p50 0.831 ms, p99 8.575 ms under uncontrolled load |

Early reduction is measured from the start of the second full period through the
first half second, and is reported separately from settled reduction. Neither
measures capture-to-display latency. The 260 cases are the 200 earlier checks plus
60 HSV cases covering the supplied HSV values, primary hue swaps, saturation-only
changes, a changing foreground hue, and dark blue/green swaps. Saturation coverage
exists because the previous grayscale projection failed all 12 saturation cases,
adding up to 53% more RGB variation after settling.

### Architecture baseline — `0.3.0-alpha.1`

The figures below are the original architecture measurement. They are retained for
comparison and are **not** a measurement of the current build. Archived
[raw results](../experiments/runs/2026-09-06_surface-frequency-v1-final/).

| Check | Result |
| --- | --- |
| Optimized surface build, all embedded shaders, risk integrator | Pass |
| Focused GPU tests | 119/119 pass |
| Canonical 640×360, 60 FPS replay | Pass |
| Canonical full-screen flash reduction | 96.63% (existing replay's encoded-color metric) |
| Canonical moving-flash reduction | 67.28% (same metric) |
| Minimum of the 36 perceptual cases | 78.52% |
| Flash sweep | No failed per-case flash gates |
| Large/small moving-square vacated peak | 0.00000144 / 0.00012639 normalized encoded-color error |
| 1080p GPU passes, p99 | 0.668 ms; capture/copy/presentation/display excluded |

The focused suite uses weak/full-range static flashes, continuously translating
strong and weak flashes, ordinary moving content, weak out-of-band controls,
and extra idle redraws for static and moving objects, at 60/120/144 captured FPS. In-band cases cover
5/7.5/10/15/20/30 Hz; out-of-band controls cover 2/4 Hz and 40 Hz only when the
capture rate can sample it. Moving objects follow a triangle trajectory at
240 pixels/second. The canonical sweep additionally covers frequencies such
as 12 and 25 Hz and saturated red.

Focused attenuation is measured in linear luminance after a 0.5-second warmup.
It must exceed 70%, and at least one visible high-phase sample must contain a
correct confirmed frequency. The reports also give confirmed/observed sample
counts: passing this test does **not** imply continuous confirmation or measured
first-event protection. The 5 Hz fully disappearing object has intermittent
confirmation even though its measured attenuation is strong.

The separate 1920×1080 benchmark renders 360 offscreen frames with a moving
flashing square and a textured background, then camera translation. It discards
30 warmup frames and reports p50/p95/p99 GPU query measurements in
`1080p-timing.json`. There is no live game or physical-display timing in this test.

The final idle check also verifies that a redraw cannot advance a moving
surface's geometry again: idle processing updates clocks and release only.

## Known limits and rejected probes

- The earlier wraparound stimulus teleported its object at the screen edge.
  Weak-flash tests exposed added modulation when protection changes during that
  relocation. Those failed results are preserved as `relocation-self-test.json`.
  Switching to continuous motion corrects the motion test; it does not fix
  teleports, abrupt scene changes, or arbitrary reappearance.
- Reacquiring phase history from nearby appearances improved some moving-flash
  results but regressed pan MAE beyond its gate, so that change was rejected.
  Replacing the retained extrema with each latest pair also regressed the
  relocation probes and was rejected.
- This version has local surface hypotheses, not persistent segmented object
  IDs, a full appearance bank, multilevel flow pyramids, or an oracle geometry
  backend. Ambiguous textureless surfaces, severe occlusion, fast displacement
  beyond the search range, and subpixel/small-object boundaries remain limits.
- It attenuates luminance/chroma flashes; it does not implement a dedicated new
  pattern-risk detector or establish HDR correctness. Both need further work.
- The measured vacated errors are very small in the tested cases. They are not
  a universal guarantee of no visible trails. The first strong event may also
  be attenuated when it is not part of a periodic flash.

## Reproduce

```powershell
.\scripts\build.ps1 -Mode surface
.\flashbench\surface-frequency.ps1 -OutputDir flashbench/artifacts/surface-retest
```

`surface` uses `/O2` and `FLASHGUARD_SURFACE_DEFAULT=1`. `release` uses `/O2`
without changing the normal default. The validation runner retains every report
and exit code and fails if a required check fails. It never opens a visible
flashing demonstration.
