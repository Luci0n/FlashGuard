# FLASHGUARD_REPLAY/5

Supersedes `/4` for new comparisons.

The `/4` baseline attempt exposed that its nominal smooth and snapped scroll cases were identical at the default 60 FPS because both requested 60 px/s, exactly one pixel per frame. `/5` requests 45 px/s at `motion_scale=1`, producing 0.75 px/frame at 60 FPS while preserving equal physical velocity between the two rasterization modes.

All REPLAY/4 measurement additions are retained:

- `MOTION_DIAGNOSTICS/2`
- `MOTION_REALIZATION/1`
- smooth and snapped text scrolling
- scroll-stop recovery
- saturated-red motion
- moving intrinsic flash

Flash evaluation is delegated to `FLASHGUARD_FLASH_SWEEP/5` / `WCAG_FLASH/4`. New motion metrics remain record-only for this baseline.
