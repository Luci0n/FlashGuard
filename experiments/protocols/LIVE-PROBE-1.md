# FLASHGUARD_LIVE_PROBE/1

Opt-in offscreen diagnostic for real application content that deterministic synthetic stimuli do
not reproduce. It is a qualitative isolation tool, not a pass/fail regression protocol.

## Procedure

```powershell
.\FlashGuard.exe --surface-frequency-live-probe <ignored-output-directory>
python flashbench\analyze-live-probe.py <directory>
```

The probe captures the first output of the default D3D adapter for five seconds without displaying
an overlay, using saved contrast settings. It writes source, filtered, and tone-only BMPs, detector
state binaries, and GPU metrics. The analyzer summarizes per-channel differences in encoded codes
and detector state-cell counts.

The tone-only image is the same source frame rendered with identical static tone mapping and
cleared detector history. A difference between the filtered and tone-only images isolates an
artifact introduced by temporal detection from one caused by static tone mapping.

## Validity rules

- Run the probe only while the intended scene is visible. A changed foreground application
  invalidates the intended comparison.
- Two probe recordings of a live application are **separate recordings**, not a deterministic
  before/after pair. Numerical comparison between recordings containing different content is
  invalid and must not be used as efficacy evidence.
- Reported pixel and cell counts describe the recorded frames only.

## Privacy and archiving

Captures contain screen content. They remain local in the ignored artifacts folder and are excluded
from source archives, packages, and the experiment archive. Only derived counts are archived. The
diagnostic is explicit opt-in and adds no readbacks to ordinary live rendering.
