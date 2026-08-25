# Successful 5-30 Hz flash sweep after red-memory fix

Tested commit: `1802a4e68656d432a10ce2bf6ba11060ed8d9788`

This rerun used the same `FLASHGUARD_FLASH_SWEEP/1` protocol as the preceding failed run after one targeted change: saturated-red mitigation was held through accumulated flash-risk memory.

All 24 cases reported:

```text
output general flashes/s = 0.000
output red flashes/s     = 0.000
```

Representative full-screen luminance reduction:

```text
5 Hz   0.77884692
10 Hz  0.91856065
15 Hz  0.96600902
20 Hz  0.98631819
30 Hz  0.98631819
```

Representative quarter-screen luminance reduction:

```text
5 Hz   0.69070210
10 Hz  0.90252973
15 Hz  0.93012940
30 Hz  0.95929544
```

Other replay metrics from the same GPU run include:

```text
static_mae                     0.00000575
15 Hz flash_reduction          0.96593596
moving_square_ghost_mae        0.00000144
moving_square_inside_mae       0.00018191
moving_square_edge_mae         0.00018191
bright_oblique_ghost_mae       0.00001799
bright_oblique_inside_mae      0.00413613
bright_oblique_edge_mae        0.00545421
small_moving_square_ghost_mae  0.00014270
pan_mae                        0.00016666
fast_pan_mae                   0.00016684
extreme_pan_mae                0.00016684
```

The result demonstrates the declared deterministic regression criteria on the recorded environment. It does not establish medical safety or formal certification.
