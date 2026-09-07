# HSV_CORRECTION_VALIDATION/1

Superset of `SURFACE_FREQUENCY_VALIDATION/1` that adds chromatic coverage and an early-reduction
metric. All measurement rules of the base protocol continue to apply unchanged.

## Case set

260 checks in total:

- the 200 cases already defined by `SURFACE_FREQUENCY_VALIDATION/1` (119 original focused cases and
  81 expanded regressions)
- 60 additional HSV cases at 5, 10, 20, and 30 Hz with 60, 120, and 144 captured FPS

The additional cases cover the supplied HSV pair, primary hue swaps, saturation-only changes, a
changing foreground hue, and dark blue/green swaps. Saturation-only coverage exists because a
grayscale-projected correction can amplify saturation alternation while passing luminance cases.

## Early and settled reduction

Two RGB variation reductions are reported per case:

```text
settled   measured after the detector has stabilised, as in the base protocol
early     measured from the start of the second full period through the first half second
```

Early reduction exists to expose protection that only converges late. It is reported separately and
is not substituted for the settled figure. Neither measures capture-to-display latency.

## Reported fields

`validation.json` records `passed`, `total_cases`, `hsv_cases`, `minimum_hsv_rgb_reduction`,
`minimum_hsv_early_reduction`, `gpu_ms`, `live_game_verified`, `executable_sha256`, `base_commit`,
`source_is_uncommitted_snapshot`, and `source_files`.

The 70% RGB attenuation requirement is unchanged from the base protocol.

## Interpretation

`live_game_verified: false` means no live game scene was revalidated for the run. Synthetic
validation cannot establish that every game scene is protected, and passing this protocol is not a
claim of seizure prevention.
