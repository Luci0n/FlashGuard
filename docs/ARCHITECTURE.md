# Architecture

FlashGuard is a Windows D3D11 desktop-capture overlay designed to reduce potentially hazardous temporal modulation while preserving ordinary motion as much as possible.

## Data path

```text
DXGI Desktop Duplication
    -> freshest captured desktop frame
    -> 128x72 linear-light analysis
    -> global/local/red/pattern/translation classification
    -> optional NVIDIA Optical Flow motion evidence
    -> full-resolution temporal safety shader
         -> raw source history (PreviousSource)
         -> filtered displayed history (PreviousOutput)
    -> capture-excluded click-through overlay
```

Default Instant mode has no intentional look-ahead queue. The swap chain uses flip-model presentation, a frame-latency waitable object, maximum frame latency 1, and wait-before-capture scheduling so the desktop frame is acquired as late as practical before rendering.

## Two full-resolution histories

FlashGuard deliberately keeps two different histories:

- `PreviousSource` is the previous raw desktop frame. Motion matching uses it so an already-filtered image cannot masquerade as object motion.
- `PreviousOutput` is the previous filtered image shown by FlashGuard. Active temporal protection uses it to constrain displayed frame-to-frame change.

`PreviousOutput` is always sampled at the same screen coordinate. Optical flow is used only as classification evidence; it never spatially warps displayed history.

## Analysis and hazard state

The GPU analyzer operates at 128x72 in linear light. It tracks current transition state separately from accumulated flash-risk memory. Evidence includes:

- signed luminance change
- affected area and directional coherence
- broad/global change
- local high-energy change
- saturated-red transitions
- repeating-pattern evidence
- translated-motion evidence

Broad/global protection and overload fallback remain authoritative over motion bypass.

## Temporal protection

When hazard evidence is active, the full-resolution shader can blend the current candidate toward `PreviousOutput`, apply a temporal low-pass, and enforce a symmetric luminance slew bound. Release uses a much shorter time constant so stale history converges quickly after the hazardous transition ends.

Static contrast reduction and saturated-red mitigation are applied before temporal feedback. Red mitigation is held through accumulated flash-risk memory rather than only on the transition frame.

## Motion classification

Motion must suppress stale temporal history without letting an erroneous motion vector bend the picture.

The current hierarchy is:

1. 128x72 translation/camera-motion evidence.
2. NVIDIA Optical Flow when a fresh solve is warranted.
3. Raw-source local patch matching when NVOFA is unavailable, anchor-only, or locally inconclusive.
4. CPU whole-frame camera-motion evidence supplied to the final shader.

NVOFA uses D3D11, half-resolution input, the `FAST` preset, forward and backward prediction, preferred output grids 1 -> 2 -> 4, S10.5 vectors, and optional 8-bit cost buffers. Every desktop update refreshes the immediate previous-frame anchor; expensive `NvOFExecute` calls are sparse. Temporal hints are valid only after a consecutive successful execute.

The portable local matcher verifies structure rather than trusting a center pixel. It includes cardinal, diagonal, oblique, and dense small-offset patch refinement for ambiguous bright/flat objects.

## Capture failure behavior

The overlay is click-through and non-activating and uses `WDA_EXCLUDEFROMCAPTURE` to prevent feedback. Capture faults are handled by retaining the last safe output for brief interruptions and activating a neutral fallback shield when the capture heartbeat remains unhealthy. Idle-release rendering allows a protected static frame to finish converging even if the desktop stops producing new updates.

## Engineering constraints

The design favors, in order:

1. reducing hazardous temporal modulation
2. preserving image geometry
3. suppressing saturated-red flash pairs
4. reducing motion trails and ghosting
5. minimizing capture/present latency and GPU cost
6. keeping low-resolution detector structure invisible in the final image

FlashGuard is experimental risk-reduction software. These engineering mechanisms are not proof of medical safety or formal accessibility certification.
