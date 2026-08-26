# Changelog

All notable public changes are documented here. FlashGuard follows Semantic Versioning for software releases. Test protocols and archived experiment runs are versioned independently; see `docs/VERSIONING.md`.

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
