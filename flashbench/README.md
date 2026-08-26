# FlashBench

Automated Windows/GPU validation and parameter screening for FlashGuard.

## Current tests

`run.ps1 -Mode compile` builds the application and NVOFA helper, validates embedded shaders, and validates risk-integrator invariance.

`run.ps1 -Mode gpu-smoke` additionally executes real D3D11/NVOFA and the synthetic replay. The replay measures normative flash behavior, moving/scrolling distortion, motion diagnostics, moving-flash attenuation, and perceptual trailing.

FlashBench v6 adds two important layers:

- **Perceptual trail metrics.** `trail-metrics.json` records moving-object tests that inspect the exact pixels just vacated by the object at full pixel resolution and report mean, worst-frame p95/p99, peak error, affected-area fractions, and time to clear below 1%, 2%, and 5% residual error. Whole-background MAE remains only a historical metric.
- **Low-contrast flash calibration.** `perceptual-sweep.json` measures static 5/10/15 Hz flashes across small source-code deltas and two phase offsets. It is deliberately separate from WCAG pass/fail.

## Batched configuration screening

`matrix-v3.ps1` no longer launches one complete FlashGuard process for every candidate. It writes a TSV plan and invokes `FlashGuard.exe --synthetic-replay-batch` so D3D11, shaders, and NVOFA stay initialized while temporal state is reset between configurations.

The first stage evaluates all 27 independent combinations of the three profile presets, three full-screen sensitivities, and three small-source sensitivities in one 320x180 / 30 FPS screening session. Screening shortens case durations, samples general metrics more sparsely, and keeps full-pixel vacated-trail measurements.

Candidates are compared by hard WCAG constraints plus a Pareto frontier over flash attenuation and perceptual motion damage. There is no weighted magic score. Full matrix runs verify only the screen winner and production default at canonical 640x360 / 60 FPS, including the low-contrast sweep.

Normal `targeted` GPU CI runs the 27-candidate screen automatically. `full` or `-TuneMatrix` also performs canonical finalist verification.

GitHub workflows:
- `FlashGuard Build`: runs automatically on GitHub-hosted Windows for every `test` push.
- `FlashGuard GPU Smoke`: targets `[self-hosted, Windows, X64, flashguard-gpu]`.

## One-time GPU runner setup

From an authenticated Windows checkout of `test`:

```powershell
git pull origin test
gh auth login
powershell -ExecutionPolicy Bypass -File .\flashbench\setup-runner.ps1
```

For GPU/desktop work, do not install the runner as a Windows service. Keep it in the logged-in desktop session so Desktop Duplication and replay can use the interactive GPU session.

```powershell
.\flashbench\start-runner.ps1
```

Keep the self-hosted runner scoped to trusted `test` branch code.
