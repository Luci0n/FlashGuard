# FLASHGUARD_REPLAY/1

## Purpose

Exercise the actual FlashGuard D3D11 safety/render path with deterministic synthetic frames and quantify flash suppression, image preservation, motion ghosting, and camera-pan behavior.

## Common environment

- replay resolution: 640x360
- frame rate: 60 FPS
- same embedded shaders and history resources as the application safety path
- NVOFA used according to the application's scheduler when supported

## Cases

- static gray control
- 15 Hz full-screen dark/bright flash
- straight bright square motion
- shallow oblique bright square motion
- small slow medium-contrast square motion
- procedural pan at normal, fast, and extreme speeds

## Main metrics

- `static_mae`
- `flash_reduction`
- `moving_square_ghost_mae`
- `moving_square_inside_mae`
- `moving_square_edge_mae`
- `bright_oblique_*_mae`
- `small_moving_square_ghost_mae`
- `pan_mae`, `fast_pan_mae`, `extreme_pan_mae`
- camera-motion statistics
- NVOFA execution counts

The exact stimulus implementation and pass thresholds are defined by the source at the tested Git commit. Material semantic changes require a new replay protocol version.
