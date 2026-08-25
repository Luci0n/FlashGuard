# Changelog

All notable public changes are documented here. FlashGuard follows Semantic Versioning for software releases. Test protocols and archived experiment runs are versioned independently; see `docs/VERSIONING.md`.

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
