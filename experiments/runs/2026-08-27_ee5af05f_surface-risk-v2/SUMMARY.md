# Surface-validated risk-only v2 — GPU #86

- Source commit: `ee5af05f14959ce1b5d200839587e7698bc58387`
- Workflow run: `33041135146` / GPU Smoke #86
- Artifact: `9634079271` / `sha256:de05d52d688560a85aa16a4999a6d6450dc1d3c4600cefb16e905c244540feec`
- Matrix: `FLASHGUARD_MATRIX/8`, targeted 320×180 / 30 FPS screen
- Workflow: **SUCCESS**
- Architecture verdict: **REJECTED** (no candidate passed all hard gates)

## Result

Mode 3 is a large improvement over failed risk-only v1, but it does not eliminate the trail mechanism.

- moving trail area >5% mean: `0.721034` → `0.118453` (~83.6% reduction)
- recovery area >5% AUC: `0.886793` → `0.060233` (~93.2% reduction)
- pan MAE: `0.018416` → `0.003192` (~82.7% reduction)
- moving-flash reduction: `0.236299` → `0.338981`

The strongest mode-3 compromise was `surface_risk_low_contrast_neutral12`:
- weak-flash minimum reduction `0.736277` (**passes ≥0.70**)
- moving-flash reduction `0.351308` (**fails ≥0.45**)
- pan MAE `0.003948` (**passes <0.010**)
- trail p99 frame p95 `0.273070` (**fails ≤0.05**)
- clear-to-5% `800.000 ms` (**fails ≤70 ms**)

## Interpretation

The residual trail is consistent with **false current-event seeding at disocclusions**, not transported displayed luminance. When a bright moving object vacates a pixel, the raw appearance change can produce `directIntrinsicEvent`; mode 3 then stores that event into `protectionStateRisk`. On the next frame the revealed background is stable, so that newly seeded risk persists on the background and decays over hundreds of milliseconds.

Optical flow was active in the screening replay (`moving_flow_frames` > 0); screening simply does not aggregate the full-resolution diagnostic MRT statistics, which is why the archived motion-diagnostics files for screen candidates are zero-filled.

## Next experiment

Keep mode 0 untouched. In benchmark mode 3 only:
1. veto current intrinsic/event seeding on verified vacated/infill disocclusion;
2. write `protectionStateRisk` from the surface-validated current event plus transported surface risk, not the generic `directIntrinsicEvent/eventMask` state update;
3. test gain up to 1.2 with neutral 0.12 after the false disocclusion seed is removed;
4. collect mode-3 moving-square diagnostics in the screen so the disocclusion/current-event gates are observable.
