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
- Do not touch `main` unless the user explicitly requests promotion. Promotion uses CrowBridge's guarded squash flow.

Always re-read the current `test` HEAD and exact affected source immediately before preparing a patch. CrowBridge uses `BASE_MODE: CURRENT_HEAD`, but GitHub should still be checked before and after each request.

## CrowBridge

The active worker runs on the Oracle Linux VM:

- service: `crowbridge.service`
- watcher: `/opt/crowbridge/crowbridge.py`
- Drive queue: `CrowBridge Queue.txt`
- Drive result: `CrowBridge Result.txt`

FlashGuard is allowlisted as `Luci0n/FlashGuard -> /opt/crowbridge/flashguard`. Normal patch requests allow only branch `test`; `main` is promotion-only.

Normal request format:

```text
CROWBRIDGE/2
STATUS: READY
REPO: Luci0n/FlashGuard
BRANCH: test
BASE_MODE: CURRENT_HEAD
COMMIT_MESSAGE: Short description

---PATCH---
<standard unified Git diff>
---END PATCH---
```

Use real `diff --git`, `---`, `+++`, and contextual `@@` hunks. Do not use zero-context patches. After writing the queue, read `CrowBridge Result.txt`, then independently verify the resulting `test` commit on GitHub. If the result is stale but GitHub clearly advanced from the request, GitHub branch state is the stronger confirmation. If a corrected request is deduplicated, alter the commit message/fingerprint.

## Build/test path

The Windows build is:

```text
scripts\build.bat release
```

It produces the root-level `FlashGuard.exe`. Keep build outputs, logs, generated QA frames/videos, benchmark artifacts, and executables out of Git.

CrowBridge runs on Linux and cannot exercise D3D11, Desktop Duplication, or NVOFA. Windows/GPU behavior still requires a Windows-side test worker or a user test until FlashBench/replay automation is completed. Do not claim an unrun Windows build or GPU test succeeded.

When a build log is provided, fix the exact compiler error before changing unrelated behavior. Previous MSVC failures included C2026 from oversized embedded HLSL literals; keep raw HLSL chunks comfortably below the compiler string-literal limit.

## Current rendering architecture

FlashGuard is a D3D11 Desktop Duplication overlay with a temporal flash limiter.

Important current properties:

- analysis grid: `128x72`
- full-resolution raw source and filtered-output histories are separate
- F9 diagnostics texture: `560x520`
- idle release renders briefly after desktop updates stop, so filtering does not remain frozen until cursor movement
- NVOFA is dynamically loaded from `nvofapi64.dll`
- NVOFA input runs at half desktop resolution with `FAST` preset
- output grid preference: `1`, then `2`, then `4`
- forward and backward flow are requested
- optional 8-bit NVOFA cost buffers are used when supported
- flow vectors are S10.5 (`/32`) and are scaled from half-resolution input coordinates back to output coordinates
- NVOFA is a motion/occlusion classifier only: it must not spatially warp `PreviousOutput`
- verified motion suppresses temporal history; questionable vectors must fail closed toward filtering rather than visibly bend geometry
- current-surface and vacated/disoccluded motion are checked separately
- ordinary frames refresh the NVOFA anchor, while expensive flow solves are requested only when detector/filter state needs them

The current pass fixes temporal hints for that sparse execution policy: temporal hints are used only when the immediately preceding NVOFA update also executed successfully. The first solve after a skipped, failed, or reset solve disables temporal hints because NVIDIA defines them as coming from the previous `NvOFExecute` call.

## Observed behavior and priorities

User observations that drove the current design:

- early versions protected flashes but produced square/circular spatial artifacts
- full-resolution temporal history removed those shapes but created motion ghosting
- moving bright objects were especially prone to trails
- an earlier flow-warping design created visible geometry/rubber-sheet warping
- classifier-only flow is preferred: a wrong vector may leave some blur, but it must not geometrically warp the image
- release previously got stuck on a static desktop until cursor movement; idle-release rendering fixed that
- Quake 3 can feel slower than Outlast Trials because its sharp, high-contrast, high-update-rate frames cause more detector/classifier work; Outlast's TAA/motion blur often presents an easier temporal signal

Optimization priority is:

1. preserve suppression of hazardous temporal modulation
2. eliminate visible geometric warping
3. reduce ghosting/trails on real motion
4. minimize latency/judder and GPU cost
5. avoid detector masks becoming visible shapes

Do not bypass protection solely from flow magnitude. Keep multiple evidence sources such as forward/backward consistency, raw-frame residual/patch improvement, cost confidence, and coarse motion state.

## How to iterate

For each pass:

1. Read current `test` HEAD on GitHub.
2. Read the exact relevant `src/FlashGuard.cpp` ranges from that HEAD.
3. Identify one concrete failure mechanism; prefer measurable/correctness fixes over broad threshold tuning.
4. Check authoritative documentation when changing NVOFA/DXGI/D3D behavior.
5. Prepare one focused unified diff plus any handoff-document update.
6. Re-read `test` HEAD immediately before submitting.
7. Write one `CROWBRIDGE/2` request to `CrowBridge Queue.txt`.
8. Read `CrowBridge Result.txt` and verify the resulting GitHub commit.
9. If Windows build/runtime evidence is available, compare it with the previous iteration before making another behavioral change.

Avoid stacking several speculative motion changes into one patch; otherwise a visual improvement/regression cannot be attributed to a mechanism.

## Planned automation

The next infrastructure milestone is deterministic Windows replay/FlashBench:

- prerecorded raw game frames with exact timestamps
- replay through the same D3D11/NVOFA/safety path, not a simplified CPU clone
- per-frame timing and detector/motion metrics
- optional motion, temporal, hazard, and NVOFA-cost diagnostic outputs
- a Windows worker that notices a new `origin/test` HEAD, builds, replays the corpus on the real NVIDIA GPU, and publishes results for the LLM

Until that exists, CrowBridge removes manual source-file transfer but does not itself provide Windows GPU execution.
