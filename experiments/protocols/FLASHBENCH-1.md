# FLASHBENCH/1

`FLASHBENCH/1` is the current Windows/GPU regression orchestration protocol.

A GPU-smoke run performs, in order:

1. release MSVC build
2. embedded HLSL validation
3. NVOFA smoke helper build
4. real D3D11/NVOFA smoke execution
5. deterministic synthetic replay through FlashGuard's D3D11 path
6. 5-30 Hz flash-sweep evaluation
7. summary generation

The individual report schemas are versioned independently. The exact implementation at the tested Git commit is part of the protocol definition; archived runs therefore always record the commit.
