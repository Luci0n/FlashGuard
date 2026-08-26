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
| `2026-08-26_0c0aa304_verified-local-transport-veto` | `0c0aa30498b77bf06c83d927dfa056e0cebc56ff` | FAIL | Local-transport veto restored small-object motion metrics but reintroduced 1-2 strict transitions/s in the formerly exact 10-30 Hz quarter-screen cases |
| `2026-08-26_82208634_chromaticity-red-clamp` | `82208634053559bc3b52f040c65e36d602d406c9` | PASS | Chromaticity final-red clamp; all standards gates passed and established the pre-state-transport motion/safety baseline |
| `2026-08-26_539eee97_compact-protection-state` | `539eee97434f763b292878fd36e89b01060d1052` | PASS | Compact transported luminance state improved ordinary motion but weakened moving-flash and low-frequency quarter-screen attenuation |

The archive contains the complete root-level raw JSON recovered from each listed CI artifact. Earlier historical measurements may be described in changelog/design history, but they are not promoted to raw archived evidence unless the original artifact can be recovered.
