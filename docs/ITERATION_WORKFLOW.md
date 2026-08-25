# FlashGuard iteration workflow

This file is the handoff for a future ChatGPT/LLM instance with no conversation context.

## Repository and branch policy

- GitHub: `Luci0n/FlashGuard`
- Development branch: `test`
- Production branch: `main`
- Windows working copy: `E:\Programs\FlashGuard`
- CrowBridge Linux scratch clone: `/opt/crowbridge/flashguard`
- Normal edits go to `test` only.
- Do not write to GitHub directly. Submit repo changes through CrowBridge.
- Do not touch `main` unless the user explicitly requests promotion.

Always re-read the current exact `test` HEAD and affected source immediately before preparing a patch.

## CrowBridge

The active worker runs on the Oracle Linux VM:

- service: `crowbridge.service`
- watcher: `/opt/crowbridge/crowbridge.py`
- Drive queue: `CrowBridge Queue.txt`
- Drive result: `CrowBridge Result.txt`

FlashGuard is allowlisted as `Luci0n/FlashGuard -> /opt/crowbridge/flashguard`.

Normal request:

```text
CROWBRIDGE/2
STATUS: READY
REPO: Luci0n/FlashGuard
BRANCH: test
BASE_MODE: CURRENT_HEAD
BASE_COMMIT: <exact current test HEAD>
COMMIT_MESSAGE: Short description

---PATCH---
<standard contextual unified Git diff>
---END PATCH---
```

Use real `diff --git`, `---`, `+++`, and contextual `@@` hunks. Validate hunk old/new counts before uploading. After queueing, read `CrowBridge Result.txt` and independently verify GitHub `test`. If stale, rebuild the patch against the new HEAD and resubmit.

## Windows build and test path

Release build:

```text
scripts\build.bat release
```

Output:

```text
FlashGuard.exe
```

Embedded shader validation:

```text
FlashGuard.exe --validate-shaders
```

Full GPU regression:

```powershell
powershell -ExecutionPolicy Bypass -File .\flashbench\run.ps1 `
    -Mode gpu-smoke `
    -OutputDir .\flashbench\manual-results
```

Visual replay:

```powershell
powershell -ExecutionPolicy Bypass -File .\flashbench\run.ps1 `
    -Mode gpu-smoke `
    -OutputDir .\flashbench\manual-results `
    -VisualReplay

Start-Process .\flashbench\manual-results\visual\index.html
```

Important reports:

```text
summary.json
nvof-smoke.json
synthetic-replay.json
flash-sweep.json
flashbench.log
```

Keep build outputs, logs, generated replay frames, artifacts, and executables out of Git.

When changing embedded HLSL, remember MSVC C2026: split long `R"HLSL(... )HLSL"` literals before they approach the compiler string-literal limit.

## CI and self-hosted GPU runner

Every `test` push currently triggers:

- `.github/workflows/build.yml`: hosted Windows release build + embedded HLSL validation.
- `.github/workflows/gpu-smoke.yml`: self-hosted `[Windows, X64, flashguard-gpu]` GPU test.

The GPU runner is an interactive logged-in Windows session because Desktop Duplication and replay depend on a real desktop. Known runner:

```text
C:\actions-runner
runner name: flashguard-CROW
```

A fully successful GPU job stages the exact tested executable to:

```text
C:\FlashGuard-Tested\<commit>\FlashGuard.exe
C:\FlashGuard-Tested\LATEST.txt
```

Do not claim GPU success unless the exact commit's GPU workflow completed successfully and, when relevant, inspect its artifact metrics.

## Current rendering architecture

FlashGuard is a D3D11 Desktop Duplication overlay with a full-resolution temporal flash limiter.

Core properties:

- analyzer: `128x72`
- diagnostics surface: `560x640`
- separate full-resolution raw source history: `PreviousSource`
- separate full-resolution filtered display history: `PreviousOutput`
- output history is sampled at the same screen coordinate; never spatially warp it
- current event state and accumulated flash-risk memory are separate
- idle release renders after desktop updates stop so filtered output can converge
- optional static contrast transform is applied before temporal feedback
- saturated-red mitigation is applied before temporal feedback and is held through flash-risk memory

The final shader uses coarse event/risk/motion from the analyzer plus full-resolution source/display deltas. Active hazards receive temporal low-pass filtering and a hard luminance slew limit. Release converges much faster to prevent stale history from turning later motion into trails.

Broad/global protection, overload fallback, and hard protection signals remain authoritative over motion bypass.

## Low-latency capture/present path

The Quake 3 testing exposed real capture-to-display latency that deterministic texture replay could not measure.

Current low-latency path:

- DXGI Desktop Duplication
- flip-model `DXGI_SWAP_EFFECT_FLIP_DISCARD`
- swap-chain frame-latency waitable object
- maximum frame latency `1`
- wait **before capture**, then acquire the freshest desktop image
- no intentional look-ahead queue in default Instant mode

This substantially reduced the whole-image lag seen in windowed Quake 3. Do not revert to simple present-after-capture queuing without measuring end-to-end latency.

## NVOFA role

NVIDIA Optical Flow is dynamically loaded from `nvofapi64.dll`.

Current policy:

- D3D11 NVOFA
- half-resolution input
- `FAST` preset
- forward + backward prediction
- output grid preference `1`, then `2`, then `4`
- S10.5 vectors (`/32`)
- optional 8-bit cost buffers
- classifier-only: **never warp displayed history**

NVOFA validates two different motion situations:

1. current surface -> its previous location
2. vacated/disoccluded pixel -> where an object used to be

Forward/backward consistency, raw patch residual, cost confidence, and coarse motion evidence are combined. A plausible vector alone is not enough.

NVOFA execution is sparse:

- every desktop update refreshes the immediate previous-frame anchor
- expensive `NvOFExecute` runs only when detector/filter state needs it
- temporal hints are enabled only after a consecutive successful execute
- a skipped, failed, or reset execute disables temporal hints on the next solve

## Anchor-only/local motion fallback

A major bug was found when NVOFA existed but had no fresh flow field:

```text
P8.x = 0.0 -> NVOFA unavailable
P8.x = 0.5 -> anchor updated, no fresh flow
P8.x = 1.0 -> fresh flow available
```

Originally `P8.x = 0.5` disabled both NVOFA classification and the portable matcher. Small moving objects could therefore be treated as hazards and blended against stale `PreviousOutput`.

Current behavior:

- fresh NVOFA -> use NVOFA evidence first
- anchor-only -> use the raw-source local matcher
- NVOFA unavailable -> use the raw-source local matcher
- verified whole-frame CPU camera motion -> avoid expensive local fallback
- weak fresh NVOFA can still be refined by the raw matcher

The portable path uses patch verification rather than trusting center-pixel color. It includes cardinal, diagonal, oblique, and dense small-offset patch refinement for ambiguous bright/flat moving objects.

CPU whole-frame camera-motion score is also passed to `PSMain`; otherwise the CPU could suppress NVOFA during a pan while the final shader still blended stale history.

## Red-flash handling

The 5-30 Hz sweep exposed a separate red issue.

Initial sweep result:

- luminance cases: general-flash output counter already passed
- saturated red at 5, 7.5, and 10 Hz: red-specific opposing transitions still leaked

Cause: red desaturation was effectively event-only. During a longer high-red half-cycle the image could become saturated again before the next opposing transition.

Current fix:

```text
red mitigation gate = max(current red event, flash-risk memory)
```

The hold is still multiplied by isolated-red evidence, so unrelated non-red content is unaffected by the red-memory path.

## Deterministic replay coverage

Replay runs the actual D3D11 safety/render path, not a CPU approximation:

```text
generated source texture
    -> analysis
    -> instant safety
    -> NVOFA when scheduled
    -> PSMain temporal/motion path
    -> PreviousOutput
    -> GPU readback
```

Current motion cases include:

- static gray control
- 15 Hz full-screen dark/bright flash
- 64x64 bright object motion
- 32x32 bright oblique motion
- 24x24 medium-contrast slow local motion
- procedural camera pans at 3, 8, and 16 px/frame

Visual mode writes sampled BMP triptychs:

```text
SOURCE | FILTERED | 6x RGB DIFFERENCE
```

and builds `visual/index.html` with case selection, frame slider, and play/pause.

## 5-30 Hz flash sweep

Current FlashBench adds 24 two-second, 60 FPS cases:

```text
frequencies:
5, 7.5, 10, 12, 15, 20, 25, 30 Hz

case families:
- luminance_full
- red_full
- luminance_quarter
```

`flash-sweep.json` records:

- `frequency_hz`
- raw/output variation
- reduction
- peak output delta
- raw/output general flashes per second
- raw/output saturated-red flashes per second

The source stimulus must actually exceed 3 flashes/s. The filtered output must be at or below 3 counted flashes/s for the applicable general/red counter.

The quarter-screen case is deliberately conservative but is **not** a formal steradian/visual-angle compliance measurement.

Latest known fully GPU-tested commit at the time of this handoff:

```text
1802a4e68656d432a10ce2bf6ba11060ed8d9788
```

Its RTX 3060 sweep passed all 24 cases with:

```text
output general flashes/s = 0.000
output red flashes/s     = 0.000
```

Representative full-screen luminance reductions:

```text
5 Hz   0.77884692
10 Hz  0.91856065
15 Hz  0.96600902
20 Hz  0.98631819
30 Hz  0.98631819
```

Representative quarter-screen reductions:

```text
5 Hz   0.69070210
10 Hz  0.90252973
15 Hz  0.93012940
30 Hz  0.95929544
```

These are deterministic engineering regressions, not medical or Harding FPA/PSE certification.

## Observed failure history

Keep these lessons when iterating:

- coarse low-resolution masks created visible squares/circles
- full-resolution temporal history solved mask shapes but introduced ghosting
- flow-warped output history caused rubber-sheet/geometry deformation
- static desktop release once froze until cursor movement
- device/present queuing made the whole Quake 3 image feel delayed
- anchor-only NVOFA state left small motion unclassified
- straight bright-motion metrics initially measured only the trail and missed errors inside the object
- cardinal/45-degree-only fallback missed shallow oblique bright motion
- saturated-red protection initially failed lower-frequency red pairs despite luminance protection

Optimization priority:

1. suppress hazardous temporal modulation
2. preserve geometry
3. suppress red-flash pairs
4. eliminate motion ghosting/trails
5. minimize capture/present latency and GPU cost
6. prevent analyzer masks from becoming visible shapes

## How to iterate

For every behavioral pass:

1. Fetch exact current GitHub `test` HEAD.
2. Fetch exact relevant source from that commit.
3. Reproduce or define one measurable failure.
4. Prefer a targeted mechanism fix over broad threshold tuning.
5. Add or strengthen a regression that would have caught the failure.
6. Build a contextual unified diff and validate all hunk counts.
7. Re-check `test` HEAD immediately before queueing.
8. Submit exactly one CrowBridge request.
9. Read CrowBridge result and verify resulting GitHub commit.
10. Follow the exact commit into hosted Windows CI and the self-hosted GPU job.
11. Inspect `summary.json`, `synthetic-replay.json`, `flash-sweep.json`, and/or logs.
12. Do not weaken existing safety/motion gates merely to make CI green.

When a new test fails, treat the failure as evidence. Fix the mechanism or explain why the metric is invalid before changing the pass threshold.

## Important remaining gaps

Deterministic replay is now strong, but it does not replace real gameplay validation.

High-value future work:

- replay real recorded Outlast/Quake stimuli with exact timestamps
- measure live Desktop Duplication frame age and end-to-end capture/present timing
- expand flash sweep below 5 Hz and/or at finer frequency increments
- add duty-cycle variation rather than only 50% square-wave phase
- add spatial patterns and different flash-area geometries
- calibrate formal visual-angle/steradian test geometry if standards conformance testing becomes a goal
- continue improving arbitrary local bright-object motion without reintroducing expensive whole-frame matching

Never describe FlashGuard as guaranteed seizure prevention or a certified medical/accessibility device.
