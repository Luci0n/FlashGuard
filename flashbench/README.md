# FlashBench

Automated Windows validation for FlashGuard.

## Current tests

`run.ps1 -Mode compile`:
- builds `scripts\build.bat release`
- runs `FlashGuard.exe --validate-shaders`
- writes `flashbench/results/summary.json` and `flashbench.log`

`run.ps1 -Mode gpu-smoke` additionally verifies the self-hosted Windows machine can see an NVIDIA GPU/driver and `nvofapi64.dll`. This is an environment smoke test; it does **not yet** claim that an optical-flow solve or replay passed.

GitHub workflows:
- `FlashGuard Build`: runs automatically on GitHub-hosted Windows for every `test` push.
- `FlashGuard GPU Smoke`: targets `[self-hosted, Windows, X64, flashguard-gpu]`.

## One-time GPU runner setup

In GitHub open `Luci0n/FlashGuard` -> Settings -> Actions -> Runners -> New self-hosted runner. Choose Windows x64 and follow GitHub's generated download/configuration commands. During configuration add the custom label `flashguard-gpu`.

For FlashGuard GPU/desktop work, **do not install the runner as a Windows service**. Start it from the logged-in desktop session so later Desktop Duplication/replay tests can use the interactive GPU session:

```powershell
.\flashbench\start-runner.ps1
```

GitHub's registration token is time-limited, so the generated setup commands must come from the repository's Runners page.

## Next milestone

Add deterministic prerecorded/synthetic replay so the GPU job executes the same D3D11/NVOFA/safety path and emits motion, hazard, temporal-mask, cost/confidence, ghosting and timing metrics.
