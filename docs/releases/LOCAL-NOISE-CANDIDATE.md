# Local noise correction — 2026-09-06

Close the previous instance and run FlashGuard.exe from this package. F9 shows
the backend; Ctrl+Shift+F12 exits; F8 enables the manual shield.

The previous noise correction checked whether the entire image changed. An
animation elsewhere could therefore let transported history create false flash
events in a stationary region. The new correction also checks actual local
color changes before accepting an event. It does not disable motion tracking,
add frame buffering, or blend previously displayed pixels.

A regression now freezes a textured scene while a separate patch continues
animating. The previous executable produced up to 33.1964 encoded color codes
of unwanted change in the stationary region after settling. This executable
reduced that to 0.00005 codes at 60, 120, and 144 FPS. These are GPU readback
measurements; a visually identical reproduction of the user's live speckles
has not yet been confirmed.

All 119 original focused cases pass. 68 of 69 expanded cases pass. The remaining
moving white/red case (5 Hz at 144 captured FPS) achieves 65.6% RGB variation
reduction, below the required 70%. The validation report deliberately remains
failed. This is an experimental test build, not completed seizure protection.
Ordinary motion can still trigger provisional attenuation, and arbitrary
imagery is not guaranteed free of trails or added modulation.

The results folder contains exact measurements and executable/source hashes.
GPU timings exclude capture, copy, presentation, and physical display latency.
Compare-Legacy.cmd selects the previous backend; close the active instance
before switching.
