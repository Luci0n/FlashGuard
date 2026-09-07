# FlashGuard - experimental low-latency photosensitivity risk reduction

FlashGuard is a Windows D3D11 overlay that captures the desktop, detects potentially hazardous flashing, and limits the displayed temporal modulation while trying to preserve ordinary motion.

It is **not medically validated, clinically epilepsy-safe, or Harding FPA/PSE certified**. Passing FlashBench is an engineering regression result, not a medical guarantee or formal accessibility certification.

Current experimental version: **0.4.0-alpha.4**.

The [surface-frequency GPU path](docs/SURFACE-FREQUENCY.md) detects 5–30 Hz surface
changes and composites the current image without NVOFA or displayed-image feedback.
As of `0.4.0-alpha.1` it passes its complete validation for the first time — 260 of
260 checks, including the moving white/red case that every earlier candidate failed.
Build with `scripts/build.ps1 -Mode surface` to make it the default in a test
executable; `--legacy` selects the previous path. A normal `release` build still
defaults to the legacy path and accepts `--surface-frequency`.

Passing that suite is deterministic engineering evidence on one GPU. It is not a
claim of seizure prevention, and the live game artifact recorded in
[`docs/OUTLAST-NOISE-DIAGNOSIS.md`](docs/OUTLAST-NOISE-DIAGNOSIS.md) has not been
revalidated since.

Public technical documentation:

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) - capture, detection, temporal filtering, and motion handling (legacy path)
- [`docs/SURFACE-FREQUENCY.md`](docs/SURFACE-FREQUENCY.md) - current surface-frequency path, correction history, and validation
- [`docs/SOLUTION-2026-09-05.md`](docs/SOLUTION-2026-09-05.md) - the design review that path implements
- [`docs/TESTING.md`](docs/TESTING.md) - regression methodology, 5-30 Hz sweep, reproducibility, and limitations
- [`docs/VERSIONING.md`](docs/VERSIONING.md) - software/protocol/run versioning and immutability rules
- [`experiments/`](experiments/) - immutable raw experiment records, including failed runs
- [`CHANGELOG.md`](CHANGELOG.md) - public version history

Per-candidate reports for the surface-frequency line, oldest first:
[noise/color](docs/NOISE-COLOR-CANDIDATE.md),
[local noise](docs/LOCAL-NOISE-CANDIDATE.md),
[Outlast diagnosis](docs/OUTLAST-NOISE-DIAGNOSIS.md),
[source amplitude](docs/AMPLITUDE-BOUND-CANDIDATE.md),
[HSV correction](docs/HSV-CORRECTION.md),
[overlay UI](docs/OVERLAY-UI.md),
[monospace and opacity](docs/MONOSPACE-OPACITY.md),
[compact menu and shader startup](docs/COMPACT-STARTUP.md).

## Current processing pipeline

```text
DXGI Desktop Duplication
    -> freshest captured desktop frame
    -> 128x72 linear-light analysis
    -> global/local/red/pattern/translation classification
    -> optional NVIDIA Optical Flow motion evidence
    -> full-resolution temporal safety shader
         - PreviousSource = raw source history for motion matching
         - PreviousOutput = filtered displayed history
         - motion gates bypass stale temporal history
         - flash gates constrain temporal modulation
    -> capture-excluded click-through overlay
```

Instant mode is the practical default. It has no intentional look-ahead queue. FlashGuard uses a frame-latency-waitable flip-model swap chain and waits before capture so it captures the freshest desktop frame when the presentation queue is ready. Desktop Duplication and the Windows compositor can still add unavoidable latency.

The safety path intentionally keeps two different full-resolution histories:

- `PreviousSource`: the previous **raw** desktop frame, used to decide whether a changed pixel is explained by motion.
- `PreviousOutput`: the previous **filtered** frame the user was shown, used by the temporal limiter during an active hazard.

This distinction is important. Motion matching must not compare against an already filtered image, and optical flow must not warp the displayed history.

## Flash protection

The 128x72 analyzer works in linear light and tracks current event strength separately from accumulated flash-risk memory. Large coherent changes, rapid reversals, saturated-red transitions, repeating patterns, and small intense local sources can authorize protection.

At full resolution, protection blends the candidate image toward `PreviousOutput` only where current hazard/risk evidence and displayed delta justify it. During an active hazardous transition, a temporal low-pass plus a symmetric luminance slew bound limits frame-to-frame change. During release, convergence is much faster so a finished flash does not leave ordinary motion dragging against stale output.

Broad/global protection remains authoritative. Motion bypass is deliberately disabled where hard global protection or overload fallback is active.

Saturated-red mitigation is applied before temporal feedback. Red desaturation now persists through the existing hazard-memory window so a lower-frequency red high phase cannot become fully saturated again before the opposing transition.

Optional static contrast reduction is also applied before temporal feedback. Default `SafetySettings` currently include:

```text
lookaheadMs                  = 0
localDeltaThreshold          = 0.10
globalDeltaThreshold         = 0.16
affectedAreaThreshold        = 0.18
strongAffectedArea           = 0.30
globalAreaThreshold          = 0.90
coherenceThreshold           = 0.70
visualFieldAreaThreshold     = 0.25
patternScoreThreshold        = 0.24
cameraMotionSuppression      = 0.32
flashEnergyThreshold         = 0.030
smallFlashAreaThreshold      = 0.008
smallFlashDeltaThreshold     = 0.25
smallFlashCoherenceThreshold = 0.85
spillExpansionCells          = 4
localGlobalSupportThreshold  = 0.035
safeRiseRate                 = 1.35 luma/second
safeFallRate                 = 1.60 luma/second
minimumProtectionTime        = 0.22 seconds
releaseTime                  = 0.45 seconds
redThreshold                 = 0.55
redDeltaThreshold            = 0.18
redAffectedAreaThreshold     = 0.15
redDesaturation              = 0.68
displayDiagonalInches        = 27
viewingDistanceCm            = 70
overloadWhiteCeiling         = 0.72
subtleToneMap                = true
blackFloor                   = 0.08
whiteCeiling                 = 0.84
```

These are engineering defaults, not medical thresholds.

## Motion handling

The main motion rule is: **verified motion should bypass temporal history, but motion evidence should never geometrically warp the image**.

FlashGuard uses several layers of motion evidence:

1. The 128x72 analyzer estimates coherent translation and a whole-frame camera-motion score.
2. On supported NVIDIA GPUs, NVOFA is dynamically loaded from `nvofapi64.dll`.
3. The legacy default runs NVOFA at half desktop resolution with `MEDIUM`, forward/backward prediction, preferred output grid `1` then `2` then `4`, and optional cost buffers. The `--latency-nvof-lite` experiment uses `FAST` and prefers grid `2`.
4. NVOFA is classification-only. Flow vectors are used to validate current-surface transport and vacated/disoccluded pixels; `PreviousOutput` always stays at the same screen coordinate.
5. The legacy default solves flow on every new captured frame. Sparse scheduling is available in the separate `--latency-adaptive-protection` experiment.
6. Temporal hints are enabled only after a consecutive successful `NvOFExecute`. A skip, failure, or reset disables hints on the next solve.
7. If NVOFA exists but there is no fresh flow field, FlashGuard falls back to raw-source local motion matching instead of leaving the frame unclassified.
8. The fallback matcher uses patch verification, including dense small-offset search for ambiguous bright/flat objects and oblique movement.
9. CPU camera-motion evidence is also sent to the final shader so broad pans do not blend stale history even when NVOFA was intentionally skipped.

This architecture is the result of several earlier failure modes: coarse masks produced visible shapes, unrestricted RGB history caused trails, and flow-warped history produced rubber-sheet geometry deformation.

## Capture and overlay behavior

FlashGuard uses:

- DXGI Desktop Duplication; no process injection
- a flip-model `FLIP_DISCARD` swap chain
- a frame-latency waitable object with maximum latency 1
- wait-before-capture scheduling to reduce capture-to-display queueing
- click-through `WS_EX_TRANSPARENT`
- non-activating `WS_EX_NOACTIVATE`
- `WDA_EXCLUDEFROMCAPTURE` to prevent feedback
- capture watchdog and automatic neutral-shield fallback
- idle-release rendering so a protected static frame can finish converging even when the desktop stops producing updates

The centered message `Automatic shield activated` appears only for the automatic capture-fallback shield. Automatic shielding starts after a persistent capture fault (about 750 ms) or a stale capture heartbeat (about 1.2 s). Brief faults retain the last safe output. After fallback, FlashGuard requires several healthy captured frames before returning to live output.

## Controls

- `F8`: toggle the persistent manual neutral shield
- `F9`: toggle diagnostics
- `F10`: open or close the settings menu (`Escape` also closes it)
- `Ctrl+Shift+F12`: exit

Settings and hotkeys persist in `%LOCALAPPDATA%\OutlastFlashGuard\settings.ini`.

The settings menu is a compact 440x360 dark window with Home, Shortcuts, and
Advanced tabs and a draggable header. Edits apply on Save changes. Menu opacity is
adjustable from 60% to 100% and defaults to 92%; protection output opacity is
unaffected. Legacy detector controls that do not tune the surface backend are hidden
when that backend is active, and profiles have been removed. JetBrains Mono is
embedded in the executable and loaded privately, so no font installation is needed.
The menu and the loading banner are excluded from capture.

While shaders compile, a top-left banner shows elapsed time and progress, repainted
by its own UI thread. Compiled shaders are cached in
`%LOCALAPPDATA%\OutlastFlashGuard\shader-cache`, keyed by source, entry point,
target, flags, and compiler generation, and size- and checksum-checked on load. A
missing, invalid, or unwritable cache falls back to normal compilation. In the
recorded startup check, an 11-shader cold compile of 143.420 s became a 0.324 s
cached load with zero recompiles; this measures GPU and shader setup only, not
complete capture and presentation startup.

`--settings-preview` and `--startup-preview` open those surfaces for development.
Neither starts capture or protection, and the settings preview does not save edits.

The current diagnostics surface is `560x640` and includes luminance/delta, affected area, coherence, region/visual-field metrics, motion explanation, red/pattern information, state/strength, buffering, and timing-related diagnostics.

## Build

Requires Visual Studio 2022 with **Desktop development with C++**.

From the repository root:

```powershell
.\scripts\build.bat release
```

Or build a test executable that defaults to the surface-frequency path:

```powershell
.\scripts\build.ps1 -Mode surface
```

`surface` uses `/O2` with `FLASHGUARD_SURFACE_DEFAULT=1`; `release` uses `/O2`
without changing the default path. Both compile `src/FlashGuard.rc` for the
embedded font resources.

Output:

```text
FlashGuard.exe
```

Validate all embedded HLSL entry points:

```powershell
.\FlashGuard.exe --validate-shaders
```

Run on the monitor under the mouse pointer:

```powershell
.\FlashGuard.exe
```

Or choose the monitor containing a visible window:

```powershell
.\FlashGuard.exe --title "part of the window title"
```

## FlashBench

`flashbench/` contains the deterministic Windows/GPU regression suite.

Run the complete GPU suite locally:

```powershell
powershell -ExecutionPolicy Bypass -File .\flashbench\run.ps1 `
    -Mode gpu-smoke `
    -OutputDir .\flashbench\manual-results
```

It performs:

- release MSVC build
- embedded HLSL validation
- real D3D11/NVOFA execution
- deterministic synthetic replay through the same D3D11 safety/render path
- motion/ghosting regressions
- camera-pan regressions
- standards-oriented 5-30 Hz flash sweep

Important report files:

```text
flashbench/manual-results/summary.json
flashbench/manual-results/nvof-smoke.json
flashbench/manual-results/synthetic-replay.json
flashbench/manual-results/flash-sweep.json
flashbench/manual-results/flashbench.log
```

### Visual replay

To inspect the synthetic cases yourself:

```powershell
powershell -ExecutionPolicy Bypass -File .\flashbench\run.ps1 `
    -Mode gpu-smoke `
    -OutputDir .\flashbench\manual-results `
    -VisualReplay

Start-Process .\flashbench\manual-results\visual\index.html
```

The viewer shows sampled replay frames as:

```text
SOURCE | FILTERED | 6x AMPLIFIED DIFFERENCE
```

Cases include the 15 Hz flash, straight bright motion, oblique bright motion, small-object motion, and camera pan.

## 5-30 Hz flash sweep

FlashBench currently tests 24 two-second cases at 60 FPS:

```text
frequencies:
5, 7.5, 10, 12, 15, 20, 25, 30 Hz

stimuli:
- full-screen luminance flash
- full-screen saturated-red flash
- quarter-screen luminance flash
```

The sweep counts completed opposing transition pairs and records:

- raw/output variation
- peak output delta
- general flashes per second
- saturated-red flashes per second
- reduction ratio

The regression gate requires source stimuli above 3 flashes/s to be reduced to at most 3 counted output flashes/s for the applicable general/red counter.

On the self-hosted RTX 3060 run for commit `1802a4e68656d432a10ce2bf6ba11060ed8d9788`, all 24 sweep cases produced `0.000` counted output general flashes/s and `0.000` counted output red flashes/s under this deterministic screen-mean test.

Some representative modulation reductions from that run:

```text
full luminance:
5 Hz   77.88%
10 Hz  91.86%
15 Hz  96.60%
20 Hz  98.63%
30 Hz  98.63%

quarter-screen luminance:
5 Hz   69.07%
10 Hz  90.25%
15 Hz  93.01%
30 Hz  95.93%
```

The red-flash pass criterion is based on the red-flash transition counter, not on requiring a high luminance-modulation reduction. This matters because saturated red can be made safer by chromatic mitigation even when screen-mean luminance changes less dramatically.

The sweep is **standards-oriented regression testing**, not formal WCAG/Harding certification. The quarter-screen case uses a simple screen-area stimulus and does not reproduce a calibrated steradian/visual-angle laboratory measurement.

The paired-transition, saturated-red, and visual-field concepts are informed by:
- [WCAG 2.2: Three Flashes or Below Threshold](https://www.w3.org/WAI/WCAG22/Understanding/three-flashes-or-below-threshold)
- [ITU-R BT.1702](https://www.itu.int/rec/R-REC-BT.1702/)

## Automated CI

On pushes to `test`:

- `.github/workflows/build.yml` runs a hosted Windows release build and HLSL validation.
- `.github/workflows/gpu-smoke.yml` runs on the interactive self-hosted Windows runner labeled `flashguard-gpu`.
- Successful GPU runs publish per-commit test artifacts containing the machine-readable FlashBench reports.

## Repository layout

- `src/` - FlashGuard C++/embedded HLSL, including `analysis/`, `shaders/`, `render/`, and `ui/`
- `assets/` - embedded fonts and their licenses
- `scripts/` - Windows build entry points
- `flashbench/` - GPU smoke, replay, visual viewer, and regression automation
- `docs/` - public architecture, testing, and versioning documentation
- `experiments/` - immutable experiment protocols and archived raw results
- `VERSION` / `CHANGELOG.md` - software version and public change history
- `.github/workflows/` - hosted build and self-hosted GPU CI

## Known limitations

- This project reduces measured temporal modulation in its regression corpus; it cannot guarantee seizure prevention for every person or every stimulus.
- Desktop Duplication and Windows composition still impose some latency even with the waitable low-latency path.
- Real gameplay can expose motion/content combinations not represented by deterministic synthetic cases.
- The live speckle artifact recorded in `docs/OUTLAST-NOISE-DIAGNOSIS.md` was traced to temporal detection but remains unresolved, and has not been revalidated since the surface path began passing its synthetic suite.
- On the surface path, provisional attenuation can still affect ordinary color changes before a frequency is confirmed.
- The local motion fallback is deliberately bounded; unusual large or complex local motion can still be misclassified.
- NVOFA availability and behavior depend on supported NVIDIA hardware/driver/runtime.
- Luminance/chroma limiting can alter colors, highlights, shadows, and perceived contrast.
- Display-size/viewing-distance calibration is approximate.
- Pattern detection and the flash sweep are not Harding FPA/PSE certification implementations.
- The detector can miss stimuli below its spatial, temporal, color, or luminance thresholds.
