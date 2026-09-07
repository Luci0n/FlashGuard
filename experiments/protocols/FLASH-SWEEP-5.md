# FLASHGUARD_FLASH_SWEEP/5

Preserves the deterministic `/4` stimulus families and frequencies.

The standards-facing output representation remains B8G8R8A8_UNORM-equivalent.

The material correction from `/4` is spatial: SC 2.3.2/G19 is no longer inferred from frame-average display luminance. Each sampled display pixel is tracked independently over the two-second trace and the maximum transition count in any one-second window is reported.

This prevents unrelated spatial changes from manufacturing an apparent light/dark oscillation in the mean while still allowing any sampled flashing component to determine the result.

SC 2.3.1 general and saturated-red output calculations use quantized display RGB. The calibrated solid-angle branch is preserved.

The legacy internal R16 epsilon reversal count is retained as a non-normative diagnostic.

The source stimulus must exceed six G19 transitions in one second for the SC 2.3.2 branch to be considered exercised.

This deterministic synthetic regression does not scan every display pixel or arbitrary video and is not external WCAG or medical-safety certification.
