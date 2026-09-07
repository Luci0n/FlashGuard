# Source-amplitude correction — 2026-09-07

Close the previous instance, then run FlashGuard.exe from this package.
Ctrl+Shift+F12 exits; F8 enables the manual shield. F9 shows the backend.

This build bounds temporal correction using actual source-frame changes.
Independent spatial medians measure luminance and RGB modulation. Small
jitter clears stale correction authority; coherent moving boundaries retain
transported amplitude. Bright-phase attenuation and color neutralization
cannot use an arbitrarily dark historical match to amplify small fluctuations.
The static contrast setting is applied to the filtered and reference colors
before the final RGB correction limit. There is no additional frame queue or
blending of previously displayed images.

The earlier spatial attenuation filters were removed because they weakened
moving color flashes. The low-frequency provisional hold now spans two 5 Hz
periods; this can also prolong temporary attenuation after an ordinary change.

199 of 200 focused checks pass (119 original and 80 of 81 regressions).
The remaining failure is moving white/red at 5 Hz and 144 captured FPS:
66.566% RGB variation reduction, below the unchanged 70% requirement.
The overall validation remains FAILED. Random-grain checks limit added
change to 3.986 encoded channel codes for a four-code source range; these
checks establish an amplification bound, not zero added grain.
Validation results and source/executable hashes are included in results.
The tests cover stationary and moving 5–30 Hz flashes, white/color changes,
weak flashes, texture, random grain, motion into colored backgrounds, and
stationary regions beside animation. Grain has a four-code source range; the
regression checks that filtering does not amplify it beyond that range.

The user's Outlast Trials scene has not yet been revalidated with this build.
Synthetic results do not establish that the live speckles are eliminated or
that this software prevents seizures. GPU timings exclude capture and display.
Private game captures are excluded from this package. Diagnostic state replay
requires the matching source-amplitude-v1 schema; old snapshots are rejected.

1080p GPU draw timing: median 0.683 ms, p99 1.980 ms (330 samples).
Canonical replay exited successfully; detailed sweep gates are in validation.json.
