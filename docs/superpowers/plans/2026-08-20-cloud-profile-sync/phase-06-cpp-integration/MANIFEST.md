# Phase 06 — C++ Integration and Lifecycle

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Connect the engine to the game: a facade, the three sync points, and a visible state.

| Packet | Depends on | Owns |
|---|---|---|
| P06-M01 | P02-M05, P03-M04, P04-M04, P05-M02 | C++ facade over the C ABI, including the pending-restore apply and discovery-status wrappers |
| P06-M02 | M01 | startup pull before the profile config is read |
| P06-M03 | M02 | post-save and exit push |
| P06-M04 | M03 | sync indicator and skip-to-offline |

Exit: the game starts, syncs, and exits on every target with no socket call on the main thread.

P06-M01 Windows checkpoint: `zig build test-cloudsync-facade -Dtest-mode=run`
passes both run modes (library absent → every call degrades; library present
→ full surface callable, credential-less Begin failing cleanly);
`install-game` stages `CloudSync.dll`; Linux cross-targets compile. Commit
`c8819e9b5`.

Carried forward from P06-M01:

- **The facade loads the library at runtime** (LoadLibrary/dlopen), which is
  what makes "a build without CloudSync still links" true. Only
  `ApplyPendingRestore` bypasses the availability notion — a staged restore
  must finish with the feature off; a missing *library* makes it a quiet
  no-op returning 0.
- `Begin(profile)` hides the pair-vs-sync decision: a sync whose failure is
  the exact `NotPaired` text retries once as a pairing on the same facade
  handle. Callers never see the fallback.
- The fingerprint is derived facade-side from the redacted credentials
  document (endpoint/bucket/url, no secrets); only self-consistency
  matters, and Zig's `creds.fingerprint` is not consulted.
- The facade is C-runtime-only (no STL) so its test binary stays out of the
  MSVC RuntimeLibrary fight; keep it that way when P06-M02 compiles it into
  the game.
- `-fentry=main` binaries never get argv — the CRT is bypassed. Modes and
  parameters for such test exes travel by environment variable.
- `CloudSyncFacade.cpp` is compiled only by its test so far. P06-M02 owns
  adding it to the game build (Main.vcxproj or the exe's source list)
  together with the first call site.
