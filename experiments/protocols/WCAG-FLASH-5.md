# WCAG_FLASH/5

## Representation

Standards-facing output measurements use B8G8R8A8_UNORM-equivalent RGB. Sub-LSB R16 temporal-history variation is excluded from pass/fail.

## Flash definitions

General flashes use opposing temporal-extrema changes in relative luminance of at least `0.10` with the darker endpoint below `0.80`. Saturated-red flashes use the existing red-ratio and CIE 1976 UCS transition test.

## SC 2.3.1

No more than three general flashes and no more than three red flashes in a one-second window passes. The calibrated small-area branch is also preserved.

## SC 2.3.2

The same general/red flash counts must remain at or below three, but there is no area exemption.

G19 is treated as a sufficient technique, not as an extra rule that turns every one-code 8-bit direction reversal into a flash. Internal R16 epsilon reversals remain diagnostic only.

This profile covers FlashGuard's declared synthetic corpus and is not W3C certification or a medical-safety guarantee.
