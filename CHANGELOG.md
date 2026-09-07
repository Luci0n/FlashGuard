# Changelog

All notable public changes are documented here. FlashGuard follows Semantic Versioning for software releases. Test protocols and archived experiment runs are versioned independently; see `docs/VERSIONING.md`.

## [0.4.0-alpha.4] - 2026-09-07

Compact settings menu, embedded fonts, and cached shader startup.

### Added

- JetBrains Mono Regular/Bold are embedded as executable resources and loaded privately, so no font installation or download is required. License and provenance are kept under `licenses`.
- A persistent compiled-shader cache under `%LOCALAPPDATA%\OutlastFlashGuard\shader-cache`. Cache keys include source, entry point, target, flags, and compiler generation; cached files are size- and checksum-checked. A missing, invalid, or unwritable cache falls back to normal compilation and does not prevent startup.
- `src/render/ShaderCache.h` and `src/ui/BundledFonts.h`; `src/FlashGuard.rc` is now compiled by `scripts/build.bat`.

### Changed

- The settings menu is 440x360 instead of 560x520. Profiles are removed.
- The menu shortcut is temporarily released while a custom shortcut is being recorded, so the recorder can capture the same key.

### Validation

- Surface build and embedded font resources verified; the compact Home layout and the absence of profiles were checked in preview.
- Two shader-startup checks compiled 11 shaders in 143.420 s, then loaded all 11 with zero recompiles in 0.324 s. This measures GPU/shader setup only, not complete capture and presentation startup.
- Automated F10 input was blocked by concurrent user input, so a completed automated menu toggle cycle is not claimed; the hotkey registration/close/reopen paths were reviewed by inspection.
- The flashing suite was not rerun for this change. The embedded protection HLSL is unchanged from `0.4.0-alpha.1`.

## [0.4.0-alpha.3] - 2026-09-07

Monospace menu and configurable menu opacity.

### Changed

- The menu and loading banner use Consolas monospace.
- Menu opacity defaults to 92% with a 60-100% slider that previews immediately and is persisted. The loading banner reuses the saved opacity on the next start. The slider is on Home in surface mode and Advanced in legacy mode. Protection output opacity is unaffected.

### Fixed

- Status-label redraw in the settings window.

### Validation

- The surface executable compiled and linked; monospace rendering, the 92% default, and slider transparency were visually verified in preview before the status-label paint correction.
- Protection shader sources match the `0.4.0-alpha.1` 260-case build; those tests were not repeated for this appearance change.

## [0.4.0-alpha.2] - 2026-09-07

Overlay settings UI and startup banner.

### Added

- A compact dark Home / Shortcuts / Advanced settings menu with a draggable header, opened with F10 and closed with Escape. Existing custom hotkeys are preserved and edits apply on Save changes.
- A top-left loading banner with elapsed time and a progress bar, repainted by a separate UI thread so it keeps updating during shader compilation.
- Developer preview commands `--settings-preview` and `--startup-preview`. Neither starts capture or protection, and the settings preview does not save edits.

### Changed

- Legacy detector controls that do not tune the surface backend are hidden when that backend is active.
- Settings and the live banner remain excluded from capture.

### Validation

- Surface build passed; Home, Shortcuts, and Advanced were visually checked in the settings preview and Escape close passed.
- The loading preview completed while its calling thread was deliberately blocked. The banner was not exposed to the screenshot tool, so no banner screenshot is claimed.
- Live protection was not enabled during UI verification. The HSV protection shaders are unchanged from `0.4.0-alpha.1`.

## [0.4.0-alpha.1] - 2026-09-07

Per-channel phase floor (HSV correction). **First fully passing validation of the surface-frequency architecture.**

This version opens a new MINOR line because the surface-frequency detector reaches its first complete validation pass, closing the moving white/red failure that every `0.3.0` candidate carried.

### Fixed

- Saturation-only flashing is no longer amplified. The previous grayscale correction failed all 12 saturation cases, adding up to 53% more RGB variation after settling. The build now stores a per-channel low phase and clips current RGB against it instead of relying on a historical luminance peak and a grayscale projection.
- Encoded RGB event detection covers dark blue/green changes, and coherent changes above approximately 6.4 channel codes can provisionally activate on the first observed transition.
- The moving white/red case at 5 Hz and 144 captured FPS, which failed in `0.3.0-alpha.2` through `0.3.0-alpha.4`, now reaches 74.17% RGB variation reduction against the unchanged 70% requirement.

### Changed

- The added state holds raw source phase bounds, not displayed image history. No frame queue is introduced. Frequency confirmation remains fixed at 5-30 Hz and the source-amplitude correction limit from `0.3.0-alpha.4` stays in place.
- Diagnostic snapshots use the `rgb-phase-floor-v2` layout; older layouts are rejected rather than reinterpreted.

### Validation

- All 260 checks pass: the original 200 cases plus 60 HSV cases at 5, 10, 20, and 30 Hz with 60, 120, and 144 captured FPS, covering the supplied HSV values, primary hue swaps, saturation-only changes, a changing foreground hue, and dark blue/green swaps.
- Minimum HSV variation reduction was 99.24% settled and 99.38% early, where early reduction is measured from the start of the second full period through the first half second.
- 1080p GPU draw time: median 0.831 ms, p99 8.575 ms under uncontrolled system load. Capture and presentation are excluded; this is not an end-to-end latency measurement.
- Archived as `experiments/runs/2026-09-07_hsv-correction`.

### Known development issues

- Provisional attenuation can still affect ordinary color changes; small changes still require frequency evidence.
- The live Outlast Trials speckles from `2026-09-06_outlast-noise-diagnosis` have not been revalidated with this build. Synthetic validation cannot establish that every game scene is protected.

## [0.3.0-alpha.4] - 2026-09-07

Source-amplitude correction bound.

### Fixed

- Temporal correction is bounded by actual source-frame change. Independent spatial medians measure luminance and RGB modulation, small jitter clears stale correction authority, and coherent moving boundaries retain transported amplitude, so bright-phase attenuation and color neutralization can no longer use an arbitrarily dark historical match to amplify small fluctuations.
- The static contrast setting is applied to the filtered and reference colors before the final RGB correction limit.

### Changed

- The earlier spatial attenuation filters were removed because they weakened moving color flashes.
- The low-frequency provisional hold now spans two 5 Hz periods, which can also prolong temporary attenuation after an ordinary change.
- Diagnostic state replay requires the matching `source-amplitude-v1` schema; older snapshots are rejected.

### Validation

- FAILED overall. 199 of 200 focused checks pass (119 original, 80 of 81 regressions). The moving white/red case at 5 Hz and 144 captured FPS reaches 66.566%, below the unchanged 70% requirement. The threshold was not relaxed.
- Random-grain checks bound added change to 3.986 encoded channel codes for a four-code source range. This establishes an amplification bound, not zero added grain.
- 1080p GPU draw timing: median 0.683 ms, p99 1.980 ms over 330 samples, excluding capture and display.
- Archived as `experiments/runs/2026-09-07_amplitude-bound`.

## [0.3.0-alpha.3] - 2026-09-06

Local source-change requirement for event acceptance.

### Fixed

- The `0.3.0-alpha.2` noise correction only checked whether the entire image changed, so animation elsewhere on the desktop could let transported history create false flash events in a stationary region. Event acceptance now also requires an actual local color change. Motion tracking is not disabled, and no frame buffering or displayed-pixel blending is added.

### Validation

- A new regression freezes a textured scene while a separate patch keeps animating. The previous executable produced up to 33.1964 encoded color codes of unwanted change in the stationary region after settling; this build reduced that to 0.00005 codes at 60, 120, and 144 FPS. These are GPU readback measurements.
- FAILED overall on the same known case: 119 original and 68 of 69 expanded cases pass, while moving white/red at 5 Hz and 144 captured FPS reaches 65.587%.
- A visually identical reproduction of the reported live speckles was not confirmed by this run.
- Archived as `experiments/runs/2026-09-06_local-noise-fix`.

### Follow-up diagnosis

- `experiments/runs/2026-09-06_outlast-noise-diagnosis` established with an offscreen five-second Desktop Duplication probe that the remaining speckles in Outlast Trials are introduced by temporal detection: rendering the identical source frame with detector history cleared and identical static tone mapping removed them. Two narrow rules were tried and reverted, one of which failed 4 original and 18 expanded cases. No fixed build was produced or released, and that run does not advance the software version.

## [0.3.0-alpha.2] - 2026-09-06

Noise and color-continuity correction.

### Fixed

- One-code dither could trigger severe desaturation, transported history could manufacture events after motion stopped, and a new colored phase could erase an established flash envelope. An additional compute pass compares untransported successive observations on the GPU, residuals of at most 2.5 encoded color codes are rejected, coherent new color phases extend the existing envelope, and full-strength protection removes chroma completely.

### Changed

- The correction retains the half-resolution grid and adds no capture queue, CPU readback, or displayed-image history. Composition still uses the current image.

### Validation

- FAILED overall: 119 original cases and 68 of 69 expanded cases pass; moving white/red at 5 Hz and 144 captured FPS stays below the 70% RGB attenuation gate.
- The expanded suite reads actual GPU output in every RGB channel and covers stable textures, one-code dither, a pan that stops, white/red flashes, changing colored phases, textured flashes, moving white/red flashes, and approximately equal-luminance red/green flashes. Moving-object variation is measured in object coordinates and background contamination separately.
- The `before-regressions.json` report measured moving-color variation in screen coordinates, so its moving cases are not directly comparable with the final object-coordinate measurements; its static, noise, and color cases are comparable.
- A live candidate instance was running during final validation, so that run's 1080p figures (p50 0.690 ms, p99 5.667 ms) are not an isolated timing comparison.
- Archived as `experiments/runs/2026-09-06_noise-color-candidate`.

## [0.3.0-alpha.1] - 2026-09-06

Surface-frequency architecture: first current-frame-only detector and compositor.

This is the first implementation of the GPU architecture proposed in `docs/SOLUTION-2026-09-05.md`. It runs without NVOFA and without displayed-image feedback, alongside the retained legacy path.

### Added

- GPU features at half source resolution carrying linear RGB and a multiscale ternary Census descriptor, with a bounded structural matcher estimating current-to-previous correspondence without NVOFA.
- Per-surface-cell transported state: discrete phase timestamps, a period estimate, phase extrema, protection lifetime, and motion. State uses integer loads and is never bilinearly blended across owners.
- Complete same-polarity periods establish 5-30 Hz evidence; original linear color changes supply flash amplitude and independent chroma evidence.
- A full-resolution compositor that selects a current owner by color agreement and compresses the upper phase toward its lower envelope. It does not read previous displayed RGB, spatially warp the image, dilate old risk masks, or brighten a dark background left behind by an object.
- Provisional attenuation for strong first transitions before frequency is confirmed, plus a bounded dormant hypothesis for when the dark phase becomes indistinguishable from the background.
- Real capture timestamps drive the live detector: pointer-only updates and idle redraws create no flash observations, idle elapsed time is subtracted from the next capture interval, and gaps over 250 ms reset continuity.
- Nonblocking timestamp/disjoint queries measuring features, tracking, composite, and total GPU time. Copy, capture, queueing, presentation, and physical display delay are excluded.
- `scripts/build.ps1 -Mode surface` and `build.bat surface` (`/O2` with `FLASHGUARD_SURFACE_DEFAULT=1`); `release` builds still default to the legacy path and accept `--surface-frequency`, while a surface build accepts `--legacy`.
- `flashbench/surface-frequency.ps1` validation runner and `flashbench/analyze-live-probe.py`.

### Validation

- Passed its checked-in runner on the RTX 3060: 119 of 119 focused GPU tests, canonical 640x360 60 FPS replay, no failed per-case flash-sweep gates, and a 78.52% minimum across the 36 perceptual cases.
- Canonical full-screen flash reduction 96.63% and moving-flash reduction 67.28% on the existing replay's encoded-color metric; vacated peak error 0.00000144 (large) and 0.00012639 (small) normalized encoded color.
- 1080p GPU passes, p99 0.668 ms, excluding capture, copy, presentation, and display.
- Archived as `experiments/runs/2026-09-06_surface-frequency-v1` and `2026-09-06_surface-frequency-v1-final`.

### Known development issues

- Reacquiring phase history from nearby appearances improved some moving-flash results but regressed pan MAE beyond its gate, and replacing the retained extrema with each latest pair regressed the relocation probes. Both were rejected.
- The earlier wraparound stimulus teleported its object at the screen edge and exposed added modulation when protection changes during relocation; those failed results are preserved as `relocation-self-test.json`. Continuous motion corrects the motion test but not teleports, abrupt scene changes, or arbitrary reappearance.
- This version has local surface hypotheses, not persistent segmented object IDs, an appearance bank, multilevel flow pyramids, or an oracle geometry backend. Ambiguous textureless surfaces, severe occlusion, fast displacement beyond the search range, and subpixel/small-object boundaries remain limits.
- It attenuates luminance and chroma flashes; it does not add a dedicated pattern-risk detector or establish HDR correctness.

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
