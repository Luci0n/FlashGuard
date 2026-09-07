# Outlast Trials live noise diagnosis — 2026-09-06

The local-noise candidate did not resolve the user's fine speckles in Outlast
Trials. A five-second, offscreen Desktop Duplication probe reproduced dark
speckles on metal shelving in the actual game. Clearing detector history and
rendering the identical source frame with identical static tone mapping removed
those added speckles. This isolates an artifact introduced by temporal detection.
The particular game rendering effect responsible for the changing input has
not been established.

The first game sample changed 20,011 pixels by more than two encoded channel
codes relative to tone mapping alone, with a maximum change of 68 codes. A
subsequent game sample had 15,939 active state cells, 13,014 of them unconfirmed;
its maximum channel change was 156 codes. These are separate live recordings,
not deterministic before/after comparisons. They show that synthetic static
and one-code-dither controls were inadequate for real game rendering.

Two narrow alternatives were tried and reverted:

- Requiring agreement farther from each event did not establish a clean live
  result. Its recording contained different gameplay, so numerical comparisons
  with the first menu recording would be invalid.
- Requiring a complete period and five neighboring cells with agreeing periods
  failed four original focused cases and 18 expanded cases. It weakened moving
  flashes and changing-color flashes. Its later live recording did not contain
  the game and is excluded from effectiveness claims.

No new noise-fixed build is claimed or packaged. The existing candidate remains
experimental and known to create noise. The failed consensus rule is removed
from the production shader. The remaining work is to establish reliable
protection authority on real game sequences without losing weak/moving/color
flashes; isolated per-pixel event and period acceptance is insufficient.

## Local diagnostic

`FlashGuard.exe --surface-frequency-live-probe <ignored-output-directory>`
captures the first output of the default D3D adapter for five seconds without
displaying an overlay. It uses saved contrast settings and writes source,
filtered, and tone-only BMPs, detector state binaries, and GPU metrics. Use it
only while the intended scene is visible; a changed foreground app invalidates
the intended comparison. `python flashbench/analyze-live-probe.py <directory>`
summarizes differences. The diagnostic is explicit opt-in and adds no readbacks
to ordinary live rendering. Captures contain screen content and stay local in
the ignored artifacts folder; they are excluded from source archives/packages.
