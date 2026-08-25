# WCAG_FLASH/1

## Reference

This profile implements the flash definitions used by WCAG 2.2 Success Criteria 2.3.1 and 2.3.2 for FlashGuard's deterministic synthetic sweep.

## Temporal evaluation

A transition is measured between successive temporal extrema, not only between adjacent rendered frames. This allows a gradual filtered transition to qualify when its accumulated endpoint-to-endpoint change crosses the applicable threshold.

Qualifying transitions are paired in opposing directions. The reported flash rate is the maximum number of completed opposing pairs in any one-second window of the measured trace.

## sRGB relative luminance

Each encoded sRGB channel is converted to linear light:

- if `C <= 0.04045`, `C_linear = C / 12.92`
- otherwise `C_linear = ((C + 0.055) / 1.055)^2.4`

Relative luminance is `0.2126 R + 0.7152 G + 0.0722 B`. A general transition qualifies when the absolute endpoint change is at least `0.10` and the darker endpoint is below `0.80`.

## Saturated red

Linear-light RGB is converted to CIE XYZ and then CIE 1976 UCS coordinates:

- `u' = 4X / (X + 15Y + 3Z)`
- `v' = 9Y / (X + 15Y + 3Z)`

A red transition qualifies when either endpoint has `R/(R+G+B) >= 0.8` and the u-prime/v-prime Euclidean distance between endpoints is greater than `0.2`.

## Area calibration

For a centered rectangular flashing region with physical half-width `a`, half-height `b`, and viewing distance `d`, solid angle is:

`4 * atan((a*b) / (d * sqrt(d^2 + a^2 + b^2)))`

The implemented SC 2.3.1 below-threshold area branch is `<= 0.006` steradians for the declared synthetic rectangle and calibration.

## Limitations

This profile is intentionally narrower than a general WCAG conformance analyzer. It does not scan arbitrary spatial masks within every possible 10-degree visual field, evaluate the fine balanced-pattern exception, or validate an external captured HDMI signal. Passing it is regression evidence for the declared corpus, not W3C certification or a medical-safety guarantee.
