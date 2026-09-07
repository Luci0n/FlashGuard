# Noise/color correction candidate — 2026-09-06

Close the previous instance and run the newly packaged FlashGuard.exe. It
defaults to the surface-frequency path. F9 shows the backend and GPU time;
Ctrl+Shift+F12 exits; F8 enables the manual shield. Compare-Legacy.cmd selects
the previous backend. Detector frequencies remain fixed at 5–30 Hz.

This candidate addresses false activation from tiny image changes and loss of
flash history when a flashing object changes color. An additional compute pass
compares the actual successive images, so transported history cannot invent
new flash events on a held image. Low-level quantization residuals are ignored.
Coherent new color phases extend the existing envelope, and full-strength
protection removes chroma completely. Composition still uses the current image.

The included results record an **overall validation failure**: a moving
white/red object flashing at 5 Hz at 144 captured FPS remains below the required
70% RGB attenuation. This is an experimental test build, not completed or
validated seizure protection. The remaining leak is not hidden by a relaxed
test threshold. Ordinary motion can still trigger provisional attenuation,
and arbitrary imagery is not guaranteed free of trails or added modulation.

The regression suite now covers stable textures, one-code dither, a pan that
stops, white/red flashes, changing colored phases, textured flashes, moving
white/red flashes, and approximately equal-luminance red/green flashes. It
reads actual GPU output in every RGB channel. Moving-object variation is
measured in object coordinates and background contamination separately.

Results and source/executable hashes accompany the package. GPU timing excludes
capture, copy, presentation, physical display delay, and competing game load.
Runtime metrics are saved on exit under
`%LOCALAPPDATA%\OutlastFlashGuard\surface-frequency-last-run.json`.
