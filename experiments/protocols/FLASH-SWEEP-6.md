# FLASHGUARD_FLASH_SWEEP/6

Preserves the deterministic stimulus families and frequencies from `/5` while correcting the SC 2.3.2 pass rule.

Standards-facing output is evaluated after B8G8R8A8_UNORM-equivalent quantization.

SC 2.3.1 passes when the output has no more than three general flashes and no more than three saturated-red flashes in any one-second window, or when the calibrated flashing area is below the existing `0.006` steradian branch.

SC 2.3.2 uses the same general-flash and saturated-red definitions but has no area exemption. The output must remain at or below three general flashes and three red flashes in every one-second window.

The internal R16 epsilon reversal counter remains a non-normative engineering diagnostic. The `/5` per-pixel one-code transition experiment is not used for pass/fail and is removed from the targeted path to avoid needless replay cost.

This is deterministic regression evidence, not external certification or a medical-safety guarantee.
