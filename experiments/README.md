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
| `2026-08-25_5445a9e9_stationary-flash-motion-rejection` | `5445a9e92d5032a86ab9c4926459cc8b371bfc31` | FAIL | Motion/flash disambiguation diagnostics; preserved major motion gains but left eight microscopic quarter-screen failures |
| `2026-08-26_bbc703d2_exact-stationary-hold` | `bbc703d2f1c9e653ffdf1778c89664e73af4f1da` | FAIL | Exact first-event hold reduced quarter-screen failures from eight to two; manual red-box testing exposed small-object trailing |

The archive contains the complete root-level raw JSON recovered from each listed CI artifact. Earlier historical measurements may be described in changelog/design history, but they are not promoted to raw archived evidence unless the original artifact can be recovered.
