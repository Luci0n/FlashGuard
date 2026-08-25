# Experiment archive

This directory preserves reproducible FlashGuard test evidence independently of temporary CI artifacts.

## Rules

- Every run is tied to an exact Git commit.
- Raw result JSON is preserved unchanged from the test artifact when available.
- Each run contains `RUN.json` with environment/protocol metadata and SHA-256 hashes for the raw files.
- Failed runs are retained.
- Existing run directories are append-only; corrections create a new run or protocol version.
- Quantitative automated measurements and qualitative observations must be labeled separately.

See `docs/TESTING.md` for methodology and `docs/VERSIONING.md` for version semantics.

## Archived runs

| Run | Commit | Result | Purpose |
| --- | --- | --- | --- |
| `2026-08-25_a50412ae_flash-sweep-failed` | `a50412ae415469a9f5e6beecca3c286a7fcc416e` | FAIL | First 5-30 Hz matrix; exposed lower-frequency saturated-red leakage |
| `2026-08-25_1802a4e6_flash-sweep-pass` | `1802a4e68656d432a10ce2bf6ba11060ed8d9788` | PASS | Same protocol after red-memory mitigation fix |

The current archive starts with the complete raw artifacts still available from these runs. Earlier historical measurements may be described in changelog/design history, but they are not promoted to raw archived evidence unless the original artifact can be recovered.
