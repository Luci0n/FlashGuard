# Changelog

All notable public changes are documented here. FlashGuard follows Semantic Versioning for software releases. Test protocols and archived experiment runs are versioned independently; see `docs/VERSIONING.md`.

## [0.2.0-alpha.8] - 2026-08-26

Final-display saturated-red feedback correction.

### Fixed

- Hazardous red is now re-evaluated after temporal RGB feedback, immediately before the filtered display state is written to history, so `PreviousOutput` cannot reintroduce saturated red after the source-side clamp.
- Final red authority is suppressed by verified ordinary motion, while compensated intrinsic residual or stable repeated intrinsic evidence can still override correspondence. Raw source history remains unmodified.

### Validation

- Alpha.7 showed that increasing the pre-temporal red clamp strength alone left the 12/15/25 Hz failures unchanged while preserving the alpha.6 motion metrics.
- Geometry, luminance protection, and REPLAY/6 stimuli remain unchanged; the targeted GPU run checks red-flash suppression and motion regression together.

## [0.2.0-alpha.7] - 2026-08-26

Intrinsic saturated-red authority correction.

### Fixed

- A strong motion-compensated intrinsic residual now drives saturated-red desaturation with a nonlinear chroma gate, allowing genuinely saturated intrinsic red transitions to reach full neutralization instead of leaving residual red proportional to the old isolated-red scalar.
- Ordinary translating red content remains on the motion path because the new authority still requires compensated intrinsic residual or stable repeated intrinsic authority.

### Validation

- Geometry, luminance protection, and REPLAY/6 stimuli are unchanged.
- The targeted GPU replay and flash sweep determine whether the remaining 12/15/25 Hz red failures are eliminated without regressing scroll/pan metrics.

## [0.2.0-alpha.6] - 2026-08-26

Motion-corroborated stable-hold and residual red-flash correction.

### Fixed

- Stable repeated-flash authority is now suppressed by independent scene-level/coarse motion corroboration, so real pans and coherent object motion can keep the alpha.3 motion bypass while stationary flashes retain half-cycle protection.
- Saturated-red protection now receives a post-correspondence full-resolution authority path: compensated intrinsic residual or stable repeated intrinsic authority can desaturate the hazardous red component without treating ordinary translating red content as a flash.

### Validation

- Geometry estimation and the REPLAY/6 corpus remain unchanged.
- Targeted GPU replay and flash sweep determine whether motion regressions from alpha.5 are removed and the remaining 12/15/25 Hz red-flash failures are eliminated.

## [0.2.0-alpha.5] - 2026-08-26

Stable-half-cycle protection continuity correction.

### Fixed

- Repeated intrinsic hazard memory now keeps temporal authority across stable raw-source half-cycles, preventing noisy/global optical flow from reopening the motion bypass between opposing flash transitions.
- The continuity requires both repeated-risk memory and same-coordinate raw-source stability; repeated risk alone still cannot suppress scrolling or other continuously changing motion.
- Current-surface exact-hold veto uses the same combined intrinsic/stable protection authority.

### Validation

- Geometry estimation remains unchanged from alpha.3; this experiment changes only temporal authority continuity after an intrinsic flash has already been established.
- Targeted GPU replay and flash sweep determine whether the 5-10 Hz regional/red regressions are restored without materially regressing scrolling or stop recovery.

## [0.2.0-alpha.4] - 2026-08-26

Intrinsic-flash authority correction for the geometry-separated motion path.

### Fixed

- Current-surface optical-flow correspondence can no longer veto an exact temporal hold when the independent motion-compensated source residual still indicates an intrinsic appearance change.
- Repeated intrinsic evidence may conservatively override vacated/disocclusion history dropping, but only while both repeated-risk memory and the current intrinsic event remain present.
- Stale repeated-risk memory by itself still cannot suppress well-compensated scrolling or ordinary motion.

### Validation

- This experiment retains the `0.2.0-alpha.3` geometry estimator unchanged and changes only the handoff from intrinsic residual evidence to temporal protection authority.
- The targeted GPU replay and flash sweep on this commit determine whether flash protection is restored without sacrificing the large scrolling/ghosting improvements from alpha.3.

## [0.2.0-alpha.3] - 2026-08-26

Motion/flash correspondence architecture experiment.

### Changed

- NVIDIA optical-flow current-surface and vacated/disocclusion confidence now comes from geometric evidence: forward/back consistency, optical-flow cost, neighborhood flow coherence, spatial observability, and short surface continuity.
- Motion-compensated source residual is evaluated independently from geometry. Valid correspondence with a small residual bypasses stale displayed history; valid correspondence with a large residual remains a protectable moving intrinsic flash.
- Accumulated flash memory no longer vetoes verified correspondence.
- Raw-source alpha carries one-frame geometry confidence along matched surfaces to tolerate brief correspondence dropouts without recursively warping filtered RGB.
- Fresh NVOFA disables the photometric portable matcher; appearance matching remains a fallback only when fresh hardware flow is unavailable.
- Removed several per-pixel previous-frame photometric verification samples from the hardware-flow path.

### Validation

- This remains an experimental architecture change. The targeted GPU replay and flash sweep on this commit determine whether it is retained.

## [0.2.0-alpha.2] - 2026-08-25

First behavioral experiment in the 0.2 development line.

### Fixed

- Repeated-flash risk accumulation is now integrated as a continuous per-second rate through the existing 0.55 s exponential decay, instead of adding the full event boost once per rendered frame.
- Direction-reversal risk remains a discrete impulse, so higher refresh rates no longer weaken or multiply the reversal contribution.
- Added a deterministic 30/60/120/240 Hz risk-integrator invariance check to FlashBench validation.

## [0.2.0-alpha.1] - 2026-08-25

Current experimental development line. This version starts from the `05c2f09925b52beadc26c75604906a320b8bd671` implementation state and is not a claim that its current GPU replay passes.

### Changed

- Motion handling now includes full-resolution NVIDIA optical-flow transport verification for current surfaces, vacated surfaces, and conservative disocclusion infill.
- Current flash localization can use a motion-compensated raw-source residual instead of relying only on same-screen-coordinate luminance change.
- Exact stationary holds, repeated-flash memory, and motion bypass logic have evolved substantially from the original `0.1.0-alpha.1` baseline.
- Experiment development is now versioned explicitly: behavior-changing experimental commits advance the current prerelease version, while documentation/archive-only commits do not change the software version.

### Known development issues

- Scrolling text and slow/stuttering motion can still retain filtered history and visibly blur.
- Flash-risk accumulation is not yet fully frame-rate invariant.
- Current replay diagnostics undersample/interleave motion evidence and are scheduled for replacement in the next protocol generation.
- `FLASHGUARD_REPLAY/3`, `FLASHGUARD_FLASH_SWEEP/3`, and `WCAG_FLASH/2` are present in archived metadata but still require complete protocol documentation or supersession.

## [0.1.0-alpha.1] - 2026-08-25

Initial versioned experimental baseline.

### Added

- 128x72 linear-light hazard analysis with full-resolution temporal output limiting.
- Separate raw-source (`PreviousSource`) and filtered-output (`PreviousOutput`) histories.
- Classifier-only NVIDIA Optical Flow support with forward/backward consistency and optional cost confidence.
- Sparse NVOFA scheduling, anchor-only raw-source fallback, CPU camera-motion bypass, and dense local patch refinement for bright/oblique motion.
- Low-latency wait-before-capture presentation path with maximum frame latency 1.
- Deterministic FlashBench replay, visual replay, motion regressions, and NVOFA smoke testing.
- Standards-oriented 5-30 Hz luminance/red flash sweep.
- Public testing, architecture, versioning, protocol, and immutable experiment-record documentation.

### Fixed

- Static protected output no longer waits for cursor movement to release.
- Flow evidence no longer warps displayed history, avoiding geometry/rubber-sheet deformation.
- Anchor-only NVOFA state no longer disables local motion classification.
- Saturated-red mitigation persists through flash-risk memory; this resolved the observed 5, 7.5, and 10 Hz red-flash regression failures in the archived sweep.

### Validation baseline

The behavior represented by this version is anchored to the fully GPU-tested implementation commit `1802a4e68656d432a10ce2bf6ba11060ed8d9788`. The archived pass/fail sequence is under `experiments/runs/`.

This version remains experimental. It is not a medical device, does not guarantee seizure prevention, and is not a Harding FPA/PSE or formal WCAG conformance certification.
