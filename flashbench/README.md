# FlashBench

Automated Windows validation for FlashGuard.

## Current tests

`run.ps1 -Mode compile`:
- builds `scripts\build.bat release`
- compiles the standalone NVOFA smoke helper
- runs `FlashGuard.exe --validate-shaders`
- writes `flashbench/results/summary.json` and `flashbench.log`

`run.ps1 -Mode gpu-smoke` additionally:
- verifies the self-hosted Windows machine can see an NVIDIA GPU/driver and `nvofapi64.dll`
- creates two deterministic synthetic textured frames in D3D11
- initializes NVOFA with the same SDK 5.0 ABI, B8G8R8A8 input, R16G16_SINT output, FAST preset, forward/backward prediction and grid preference used by FlashGuard
- calls `NvOFExecute`
- reads the generated forward-flow field back to CPU and requires nonzero motion vectors
- writes `nvof-smoke.json`

This is a real NVOFA API/driver execution smoke test. It is still smaller than a full FlashGuard replay and does not measure flash suppression or gameplay ghosting yet.

GitHub workflows:
- `FlashGuard Build`: runs automatically on GitHub-hosted Windows for every `test` push.
- `FlashGuard GPU Smoke`: targets `[self-hosted, Windows, X64, flashguard-gpu]`.

## One-time GPU runner setup

From an authenticated Windows checkout of `test`, the shortest setup is:

```powershell
git pull origin test
gh auth login
powershell -ExecutionPolicy Bypass -File .\flashbench\setup-runner.ps1
```

`setup-runner.ps1` obtains a time-limited repository runner token through `gh`, downloads the current Windows x64 GitHub Actions runner, configures the custom label `flashguard-gpu`, and starts it. If the runner is already configured, it simply starts the existing installation.

For FlashGuard GPU/desktop work, **do not install the runner as a Windows service**. Keep the runner in the logged-in desktop session so later Desktop Duplication/replay tests can use the interactive GPU session. To start an already configured runner later:

```powershell
.\flashbench\start-runner.ps1
```

If automatic token acquisition is unavailable, GitHub's Settings -> Actions -> Runners -> New self-hosted runner page can provide the normal registration commands/token. The workflows do not run on pull requests; keep the self-hosted runner scoped to trusted `test` branch code.

## Next milestone

Add deterministic prerecorded/synthetic replay so the GPU job executes the full D3D11/NVOFA/safety path and emits motion, hazard, temporal-mask, cost/confidence, ghosting and timing metrics.
