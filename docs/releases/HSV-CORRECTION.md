# HSV correction — 2026-09-07

Close the previous FlashGuard instance, extract this package, and run FlashGuard.exe.
Ctrl+Shift+F12 exits. F8 enables the manual shield. F9 shows the backend.

The previous grayscale correction could amplify saturation-only flashing:
all 12 saturation cases failed, with up to 53% more RGB variation after settling.
The exact fixed HSV pair in the supplied screenshot already passed; nearby
saturation changes and dark hue changes exposed the missing coverage.

This build stores a per-channel low phase and clips current RGB against it.
It no longer relies on a historical luminance peak and grayscale projection to
suppress color alternation. Encoded RGB event detection includes dark blue/green
changes, and coherent changes above approximately 6.4 channel codes can provisionally
activate on the first observed transition. Frequency confirmation remains 5–30 Hz.
The source-amplitude correction limit remains in place to constrain added grain.

The extra state contains raw source phase bounds, not displayed image history.
There is no added frame queue. Provisional attenuation can also affect ordinary
color changes; small changes still require frequency evidence. Diagnostic snapshots
use rgb-phase-floor-v2 and older layouts are rejected.

Results include the original 200 cases and 60 additional cases at 5, 10, 20,
and 30 Hz, with 60, 120, and 144 captured FPS. The additional cases cover the
supplied HSV values, primary hue swaps, saturation-only changes, a changing
foreground hue, and dark blue/green swaps. Early reduction is measured from the
start of the second full period through the first half-second, separately from
settled reduction. It does not measure capture-to-display latency.

Synthetic validation cannot establish that every game scene is protected or
that the software prevents seizures. Outlast's live speckles have not been
revalidated with this build. Private screen captures are excluded from the package.

All 260 checks pass, including all 60 HSV cases. Minimum HSV variation reduction:
99.24% settled, 99.38% early. The formerly failing moving white/red 5 Hz at
144 FPS now achieves 74.17% (required 70%). Median 1080p GPU draw time was
0.831 ms, p99 8.575 ms under uncontrolled system load; capture and presentation
are excluded. No additional replay or end-to-end latency claim is made.
