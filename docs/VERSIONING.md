# Versioning and traceability

FlashGuard versions software, test protocols, and experimental runs separately so a result can always be traced back to the exact implementation and definition that produced it.

## Software versions

`VERSION` contains the current Semantic Versioning identifier.

During the experimental period:

```text
PATCH  implementation/build fix that preserves intended behavior
MINOR  detector/filter architecture or capability milestone
MAJOR  incompatible public behavior/API or a mature release boundary
```

Pre-release identifiers such as `-alpha.1` indicate that the software remains experimental.

While a MINOR development line is still in alpha, each behavior-affecting experiment advances the numeric prerelease identifier (`-alpha.1`, `-alpha.2`, ...). A new detector/filter architecture or capability milestone advances MINOR and resets the prerelease identifier. Documentation, archived-result, and other metadata-only commits do not change the software version.

The version bump for a behavior-affecting experiment belongs in the same commit as the behavior change so CI artifacts and archived runs observe the correct `VERSION`. `CHANGELOG.md` summarizes meaningful software changes. Git commit hashes remain the authoritative identity for an exact implementation.

## CI policy for non-executable commits

Commits that cannot change executable behavior or test semantics may include `[skip ci]` in the commit message so push-triggered build and GPU workflows do not consume a runner. Examples include a versioning-policy-only commit, prose documentation, or immutable experiment-archive bookkeeping.

Do not skip CI for changes to runtime code, shaders, build logic, replay/sweep generators, metric definitions, protocol implementations, or workflow behavior. Those commits must run the applicable validation.

## Protocol versions

Test schemas have independent stable identifiers. The repository currently contains historical and active schema identifiers including:

```text
FLASHBENCH/4
FLASHGUARD_REPLAY/5
FLASHGUARD_FLASH_SWEEP/5
WCAG_FLASH/4
NVOF_SMOKE/1
MOTION_DIAGNOSTICS/2
MOTION_REALIZATION/1
```

A protocol identifier is not considered fully documented merely because it appears in generated JSON or `experiments/manifest.json`; the matching protocol document must exist under `experiments/protocols/` before the protocol is treated as a stable comparison baseline.

Historical protocol documents remain preserved for archived runs that used earlier measurement semantics.

A material change to stimulus generation, metric definition, pass/fail criteria, or measurement semantics requires a new protocol/schema version. Old results keep their original version.

Pure formatting or tooling changes that provably do not change measurement semantics may keep the same protocol version, but should still be documented.

## Experiment/run identity

Archived runs use a directory name containing date, abbreviated commit, and purpose, for example:

```text
2026-08-25_1802a4e6_flash-sweep-pass
```

`RUN.json` records:

- exact tested commit
- software version if one existed at test time
- protocol/schema identifiers
- timestamp and CI run identifier when available
- environment metadata
- overall result
- hashes of preserved raw result files
- relationship to a preceding experiment when relevant

The commit hash, not the directory abbreviation, is authoritative. Runs from different protocol versions are not treated as direct before/after evidence unless the measurement semantics are demonstrated to be comparable.

## Immutability

Published run directories are immutable. Never overwrite an old result with a rerun.

If a result is later found invalid, retain it and mark that status in a new record or manifest revision with the reason. Do not rewrite the original raw data.

## Release evidence

A release candidate should have:

1. a clean build and shader validation
2. a successful GPU smoke test on the exact candidate commit
3. successful deterministic replay under the declared protocol version
4. successful flash-sweep results under the declared protocol version
5. archived raw results tied to the exact commit
6. documented known limitations and any qualitative observations kept distinct from quantitative measurements

A passing regression suite is release evidence, not certification of medical safety.
