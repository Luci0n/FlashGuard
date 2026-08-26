# MOTION_DIAGNOSTICS/2

Replaces x-interleaved `/1` diagnostics with three full-resolution R16G16B16A16_FLOAT replay-only MRTs.

The twelve channels exist at every pixel:

1. global flow, current-surface transport, vacated surface, disocclusion infill
2. portable matcher, combined hardware motion, effective motion, repeated-risk memory
3. verified flash override, coarse GPU motion, CPU motion prior, temporal mask

Spatial sampling stride is 1. GPU-to-CPU readback is temporally capped near 30 samples/s.
Reports include whole-frame, active-rectangle, outside-rectangle, and source-changed-pixel aggregates.
