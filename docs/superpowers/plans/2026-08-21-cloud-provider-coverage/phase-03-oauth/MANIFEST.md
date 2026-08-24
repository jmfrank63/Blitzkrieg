# Phase 03 — OAuth Backends

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Reach the consumer clouds that need a browser.

| Packet | Depends on | Owns |
|---|---|---|
| P03-M01 | P02-M04 | rc config state machine |
| P03-M02 | M01 | browser launch and callback |
| P03-M03 | M02 | token storage and refresh |

Exit: one OAuth backend authorises and completes a sync.

P03-M01 macOS checkpoint: `test-cloudsync-oauth` 7/7, written failing-first
— the driver replays a captured v1.75.0 drive sequence
(`client_id_warning` → `*oauth-islocal,…` → `*oauth-authorize,…` → done)
against a stub that asserts every request envelope, and a live-gated test
walks the real machine to the token question. Worker 10, form 9, engine
19, abi 10 + consumer, facade green; both cross-targets compile;
`install-game` builds. Commit `88791fe66`.

Findings the packet text does not carry:

- **The captured fixture's completion and error shapes come from adjacent
  captures**, not from finishing a real OAuth dance (that needs a human
  and a browser): the done reply is the same daemon completing a webdav
  create in one step, and the in-band error is its answer to a garbage
  token — both byte-shapes of the same machine, recorded in the test's
  module doc.
- **A `client_id` parameter legitimately changes the machine**: drive
  skips its shared-client-id warning when one is set, which the live test
  discovered — captures and live walks must agree on the parameters they
  send.
- **The reply's Option block is parsed by the catalogue parser through a
  synthetic one-option wrapper** rather than a second Option-JSON mapping
  in oauth.zig — catalogue.zig is outside this packet's allowlist, and
  the wrapper reuses it verbatim; `form.fieldFromOption` is the shared
  conversion the packet asked for.
- **`cloudsync.zig`'s undo-availability busy list gains
  `.awaiting_input`** — the single exhaustive switch over the worker
  state enum this packet extends, outside the allowlist but with exactly
  one correct arm (a parked question holds the operation slot). Recorded
  here like P02-M03's Lua router.
- The wait for an answer is bounded by cancel and destroy only — there is
  no honest timeout for a person reading an OAuth page — while every rc
  exchange inside the flow keeps the client's per-POST deadline. Stale
  answers are dropped when a new question parks.
- In-band error text goes through `engine.redactedText` with the
  session's credential-derived redactions before it can reach the
  question JSON or a status line: rclone echoes rejected parameters.
