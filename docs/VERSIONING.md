# Versioning and traceability

FlashGuard versions software, test protocols, and experimental runs separately so a result can always be traced back to the exact implementation and definition that produced it.

## Software versions

`VERSION` contains the current Semantic Versioning identifier.

During the experimental period:

```text
PATCH  implementation/build fix that preserves intended behavior
MINOR  detector/filter behavior or capability changes
MAJOR  incompatible public behavior/API or a mature release boundary
```

Pre-release identifiers such as `-alpha.1` indicate that the software remains experimental.

`CHANGELOG.md` summarizes public behavior changes. Git commit hashes remain the authoritative identity for an exact implementation.

## Protocol versions

Test schemas have independent stable identifiers, currently:

```text
FLASHBENCH/1
FLASHGUARD_REPLAY/1
FLASHGUARD_FLASH_SWEEP/1
NVOF_SMOKE/1
```

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

The commit hash, not the directory abbreviation, is authoritative.

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
