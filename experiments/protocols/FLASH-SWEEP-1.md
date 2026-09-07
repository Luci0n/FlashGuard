# FLASHGUARD_FLASH_SWEEP/1

## Purpose

Measure whether deterministic repetitive luminance and saturated-red stimuli from 5-30 Hz remain above the implemented completed-flash threshold after FlashGuard filtering.

## Environment

- replay resolution: 640x360
- frame rate: 60 FPS
- duration: 120 frames / 2.00 s per case
- square-wave phase duty cycle: 50%

## Frequencies

`5, 7.5, 10, 12, 15, 20, 25, 30 Hz`

## Stimuli

### luminance_full

Full frame alternates between gray 20/255 and gray 235/255.

### red_full

Full frame alternates between RGB `(8,8,8)` and saturated RGB `(235,0,0)`.

### luminance_quarter

Background remains gray 20/255. During the high phase, a centered rectangle occupying half the width and half the height (one quarter of total screen area) becomes gray 235/255.

## Counters

A flash is counted after a pair of opposing qualifying transitions completes.

General transition counter:

- absolute screen-mean relative-luminance transition >= 0.10
- darker endpoint < 0.80

Red transition counter uses the implementation's saturated-red ratio and red-transition magnitude test. Refer to the exact tested source commit for executable semantics.

## Pass criterion

The generated source must exceed 3 completed flashes/s in the applicable counter. The filtered output must be <= 3 completed general flashes/s and, for `red_full`, <= 3 completed red flashes/s.

## Scope limitation

This is a deterministic screen-mean regression protocol. The quarter-screen stimulus is not a calibrated visual-angle/steradian conformance measurement, and this protocol is not a Harding FPA/PSE or complete WCAG certification test.
