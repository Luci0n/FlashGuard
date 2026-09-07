# FLASHGUARD_UI_VALIDATION/1

Records verification of overlay UI, settings, font, and shader-startup changes. This protocol makes
**no** protection-behavior claim and never substitutes for an image-quality protocol.

## Scope

A run under this protocol establishes only that the described interface or startup change builds,
loads, and behaves as described. Protection shaders are verified to be unchanged by hash rather
than retested. When protection sources do change, the applicable image-quality protocol must be run
and archived separately.

## Recorded fields

```text
build                      build mode used for the packaged executable
exe_sha256                 packaged executable hash
source_files               path and SHA-256 of each changed source file
protection_sources_unchanged
                           true only when protection HLSL/analysis hashes match the last
                           image-quality run
```

Interface runs additionally record preview exit codes, the tabs visually checked, and whether
Escape close was verified. Appearance runs record the applied values, such as font, default
opacity, and slider range. Startup runs record menu size, embedded font verification, cached shader
file hashes, and a `shader-startup.json` array of `{ms, hits, compiles}` entries for a cold compile
followed by a cached load.

## Verification honesty rules

- A check performed by inspection is not recorded as an automated pass.
- A visual check that could not be captured is recorded as unverified rather than omitted. For
  example, a banner not exposed to the screenshot tool yields no banner screenshot claim, and
  automated hotkey input blocked by concurrent user input yields no completed toggle-cycle claim.
- Results retained from an earlier build for reference are labeled as such and are not presented as
  rerun on the current executable.

## Timing

Shader-startup measurements cover GPU and shader setup only. They exclude complete capture and
presentation startup and are not end-to-end launch measurements.
