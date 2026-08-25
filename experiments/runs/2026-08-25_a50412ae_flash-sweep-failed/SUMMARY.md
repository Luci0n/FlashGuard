# Failed 5-30 Hz flash sweep

Tested commit: `a50412ae415469a9f5e6beecca3c286a7fcc416e`

The first complete 24-case frequency matrix failed its saturated-red criterion while the general luminance counter already passed all tested frequencies.

Observed `red_full` output red flashes/s:

```text
5 Hz    5.000
7.5 Hz  7.500
10 Hz  10.000
12 Hz   0.000
15 Hz   0.000
20 Hz   0.000
25 Hz   0.000
30 Hz   0.000
```

The working hypothesis was that red desaturation was effectively transition/event-only. During the longer high-red half-cycle at lower frequencies, saturation recovered before the next opposing transition.

The subsequent implementation held red mitigation through accumulated flash-risk memory and was rerun under the same `FLASHGUARD_FLASH_SWEEP/1` protocol.

Note: the raw `summary.json` reports `replay_status: NOT_RUN` even though `synthetic-replay.json` was emitted. The overall replay process exited non-zero because the newly integrated flash sweep failed; the raw wrapper field is preserved unchanged rather than corrected retroactively.
