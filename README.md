# FlashGuard - general-purpose low-latency screen protection

FlashGuard is an experimental general-purpose photosensitivity risk-reduction overlay for Windows. Its default Instant mode performs linear-light detection and protection entirely on the GPU before presenting the current frame. CPU readback is asynchronous and used for diagnostics rather than the display decision.

It is **not medically validated, clinically epilepsy-safe, or Harding FPA/PSE certified**. Validate changes with recorded or synthetic material before live use.

## Processing pipeline

    DXGI Desktop Duplication
        -> current raw GPU frame
        -> current/previous 64x36 linear-light analysis textures
        -> GPU block-motion, coherence, pattern, and red tests
        -> ping-pong GPU luminance-safety map
        -> current geometry + regional luminance constraint
        -> overlay

Instant GPU mode is the default and adds no intentional frame queue. It compares the current analyzer texture with the preceding raw analyzer texture, updates a 64x36 permitted-luminance map, and applies that map to the current geometry in the same GPU command stream. The prior RGB image is never blended into the output. Optional 25-100 ms Predictive modes retain future-frame classification when a user prefers stronger ambiguity handling over latency. Full-resolution frames remain GPU-only.

For dramatic repeated white/black flashing, the GPU safety map also retains a short packed transition-direction history across duplicated source frames. After the first timely opposite reversal confirms an alternating strobe, both phases are held near the same permitted luminance and drift gradually toward neutral gray. This sequence clamp expires quickly when the reversals stop, and it is restricted to extreme light/dark transitions so ordinary cuts and movement do not receive the same treatment.

## Normal and protected behavior

In ordinary Instant-mode frames, output is the current raw geometry plus the selected optional static contrast transform. Near-full-screen rises and falls in frame luminance are subject to a hard screen-wide slew limit. A dramatic partial-screen change covering roughly one fifth of the display bypasses motion suppression: rising cells begin close to their previous darker luminance, hold scalar luminance history briefly, and recover gradually toward the live image. Unchanged areas remain steady and full-resolution RGB history is never retained, preventing motion trails. There is no previous-frame RGB blending, temporal RGB averaging, or always-on red desaturation.

Localized hazards use a bilinearly sampled 64x36 safety map. Only cells changing coherently in the flash direction are transition-limited; the rest of the screen remains raw. Events affecting at least 65% of the analyzer cells use the global transform instead. Both paths always render current geometry and alter luminance/chroma only. Separate rise and fall rates handle dark-to-bright and bright-to-dark transitions.

A second local detector handles small, intense light sources using connected-component size, transition energy, a calibrated 10-degree visual-field window, and future reversal evidence. Translation matching suppresses camera pans and moving hands when a shifted previous analyzer frame explains the change. Local and pattern events remain on the regional safety map; they do not toggle a whole-screen highlight transform. Regional history is discarded when unrelated moving geometry replaces a cell, preventing the safety map from leaving a screen-space trail. Whole-screen limiting is reserved for changes covering the configured broad-screen threshold, and its lifetime is independent from subsequent local events.

The previous experimental three-frame dramatic gray clamp was removed. Instead, a static low-contrast transform is now enabled for every frame. It lifts the darkest output to `0.08` and limits the brightest output to `0.84`, reducing display contrast without adding temporal history, adaptation, or extra analyzer latency.

One flash is counted only after a pair of opposing transitions completes. Repeated flashes within one second extend protection; alternating pairs increase strength and reduce the permitted transition rate further.

## Detector

The analyzer averages nine linear-light samples per cell. Instant mode compares analyzer frame N with N-1 directly on the GPU; Predictive modes additionally use future buffered statistics. The asynchronous CPU diagnostics calculate:

- global mean luminance and signed global delta
- affected, brightening, and darkening cell percentages
- directional coherence among affected cells
- largest coherent connected region and transition energy
- maximum changed area within a calibrated 10-degree visual-field window
- camera-translation explanation score
- high-contrast repeating-pattern score
- saturated-red transitions in either direction using CIE 1976 u-prime/v-prime distance
- completed flashes and alternating directions over the last second

Large coherent changes are hazardous. Balanced bright/dark changes from fast camera movement normally are not. A sufficiently large signed global mean change can still trigger protection even without the local-area path.

Default tuning is centralized in `SafetySettings` near the top of `src/FlashGuard.cpp`.

## Repository layout

- `src/` — FlashGuard C++ source
- `scripts/` — Windows build entry points (`build.bat` and `build.ps1`)
- `tests/replay-corpus/` — replay fixtures (kept out of Git when generated or large)
- `flashbench/` — FlashBench Windows/NVOFA automation

Build with `scripts\\build.bat release` (or `scripts\\build.ps1 release`). The output is `FlashGuard.exe` at the repository root; compiler intermediates are placed in `build/` and ignored by Git.

    lookaheadMs                  = 0 (Instant GPU)
    localDeltaThreshold          = 0.10
    globalDeltaThreshold         = 0.16
    affectedAreaThreshold        = 0.18
    strongAffectedArea           = 0.30
    globalAreaThreshold          = 0.65
    coherenceThreshold           = 0.70
    visualFieldAreaThreshold     = 0.25
    patternScoreThreshold        = 0.24
    cameraMotionSuppression      = 0.32
    flashEnergyThreshold         = 0.030
    smallFlashAreaThreshold      = 0.008
    smallFlashDeltaThreshold     = 0.25
    smallFlashCoherenceThreshold = 0.85
    spillExpansionCells          = 4
    localGlobalSupportThreshold  = 0.035
    safeRiseRate                 = 1.35 luma/second
    safeFallRate                 = 1.60 luma/second
    minimumProtectionTime        = 0.22 seconds
    releaseTime                  = 0.45 seconds
    redThreshold                 = 0.55
    redDeltaThreshold            = 0.18
    redAffectedAreaThreshold     = 0.15
    redDesaturation              = 0.68
    displayDiagonalInches        = 27
    viewingDistanceCm            = 70
    overloadWhiteCeiling         = 0.72
    subtleToneMap                = true
    blackFloor                   = 0.08
    whiteCeiling                 = 0.84

These are engineering defaults, not medical thresholds.

The paired-transition, saturated-red, and visual-field concepts are informed by
[WCAG 2.2 flash guidance](https://www.w3.org/WAI/WCAG22/Understanding/three-flashes-or-below-threshold)
and [ITU-R BT.1702](https://www.itu.int/rec/R-REC-BT.1702/). FlashGuard is a live
risk-reduction experiment, not a conformance tester or medical device.

## Controls

- `F8` by default: toggle the persistent manual neutral shield. The binding is editable; a one-second cooldown, no-repeat registration, and modifier-key guard prevent accidental double toggles. Turning the shield off rebuilds monitor capture and restarts the analyzer pipeline.
- `F9`: toggle the non-flashing diagnostics panel
- `F10`: open the runtime options menu
- `Ctrl+Shift+F12`: exit FlashGuard

The F9 panel shows linear luminance, delta, affected area, direction split, coherence, largest region, calibrated visual-field area, transition energy, motion explanation, pattern score, red area, completed flash count, trigger type, future frames, deadline misses, state, strength, buffer depth, and target latency.

At startup, a small non-flashing strip appears in the bottom-left corner for ten seconds and lists the active shortcuts. The diagnostics surface is tall enough to show every metric without clipping.

F10 opens a centered, focusable settings window with a dark frosted navy/violet material and releases any cursor confinement while it is open. The material is cached with the window instead of capturing or filtering the screen behind it, so moving the menu adds no refresh loop, lag, or backdrop flicker. Its presets are Performance (Instant/original contrast), Balanced (Instant/reduced contrast), and Maximum (50 ms Predictive), followed by a continuous contrast-reduction slider, full-screen sensitivity, small-source sensitivity, mode/look-ahead, display size, viewing distance, diagnostics, and hotkeys. F8 is the default editable shield toggle; one binding controls both shield states. Hover any setting or hotkey field for a plain-language explanation of its behavior and safety/performance tradeoffs. Use Tab, arrow keys, Enter, and Escape if mouse capture is inconvenient. Global shortcuts are suspended while this window is open, so existing keys can be captured without activating their old actions. An editable hotkey may be cleared to leave that action unassigned. Conflicting or unavailable hotkeys are rejected and the previous bindings restored. Settings and hotkeys persist in `%LOCALAPPDATA%\OutlastFlashGuard\settings.ini`.

## Capture and overlay behavior

The existing safety-oriented capture architecture remains:

- DXGI Desktop Duplication; no injection or window-capture API
- click-through `WS_EX_TRANSPARENT` overlay
- non-activating `WS_EX_NOACTIVATE` behavior
- `WDA_EXCLUDEFROMCAPTURE` anti-feedback requirement
- capture watchdog and gradual neutral-shield recovery behavior

The centered message `Automatic shield activated` appears only for the capture-fallback shield, never for the manual F8 shield. Automatic shielding begins when capture remains faulted for at least 750 ms or stops providing a healthy frame/timeout heartbeat for 1.2 seconds. Brief faults retain the last safe output instead. Once the fallback is visible, FlashGuard requires eight real captured frames before returning to live output.

## Build

Requires Visual Studio 2022 with **Desktop development with C++**.

PowerShell:

    .\build.ps1
    $process = Start-Process .\FlashGuard.exe -ArgumentList '--validate-shaders' -Wait -PassThru
    $process.ExitCode  # 0 means all embedded HLSL entry points compiled

Run without arguments to protect the entire monitor currently under the mouse pointer:

    .\FlashGuard.exe

The filter keeps running independently of whichever application was foreground when it started. To select the monitor containing a particular visible window and retain window-specific startup validation, use:

    .\FlashGuard.exe --title "part of the window title"

## Known limitations

- Instant GPU mode has no intentional frame queue, but Desktop Duplication and Windows composition still impose unavoidable capture/display latency. Predictive values add the selected 25-100 ms on top.
- Analyzer queries and swap-chain presentation do not block the capture path. When analysis misses its deadline, the frame receives a conservative steady highlight shoulder; this is safer than raw fallback but can briefly alter a non-hazardous image.
- Luminance transforms preserve geometry and approximate chroma, but extreme lifts/dimming can still compress highlights or shadows.
- A localized safety mask can soften across neighboring 64x36 cells because it is bilinearly interpolated to avoid hard tile edges.
- Display-size/viewing-distance calibration is an approximation, not a photometric measurement of the actual monitor or room.
- Pattern detection is deliberately conservative and is not a Harding FPA/PSE certification implementation.
- The detector can still misclassify unusual camera cuts or miss stimuli below its spatial, temporal, color, or luminance thresholds.
