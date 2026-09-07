# MOTION_REALIZATION/1

Records motion requested in physical pixels/second and the actual per-frame coordinate sequence used by `FLASHGUARD_REPLAY/4`.

The protocol exists to distinguish equal wall-clock velocity from equal per-frame displacement and to expose integer move/stall cadence.

Current arrays record smooth subpixel-scroll offsets and integer-snapped offsets together with replay FPS and motion scale.

This file is measurement provenance only; it does not define a pass/fail threshold.
