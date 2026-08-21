# P03-M01 — rc config state machine

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Drive rclone's interactive config from Zig, without a terminal.

**Dependencies:** P02-M04 — the state machine surfaces prompts the generic form renders, and its completion path runs the same connection test, so it needs validation in place, not merely rendering.

**Allowed files:** `Sources/src/CloudSync/oauth.zig`, `Sources/src/CloudSync/oauth_test.zig`, `Sources/src/CloudSync/worker.zig`, `Sources/src/CloudSync/engine.zig`, `build.zig`.

- [ ] Write the failing test against a stub rc server replaying a captured state sequence before writing the driver.
- [ ] Implement the `config/create` state loop: post with `opt.State` and `opt.Result`, read the returned `State`, `Option` and `Error`, repeat until the machine reports completion. This is the same mechanism rclone GUI wrappers use.
- [ ] Treat the state machine as opaque data. Do not interpret provider-specific states; the loop must be able to carry a backend added after this code was written.
- [ ] Surface an intermediate step that needs a value as a generic prompt built the same way P02-M01 builds a field, so the dialog can render it without new code.
- [ ] Bound every step with the existing deadline mechanism, and run it on the worker like every other rc call. **That means owning the worker changes here** — a new job kind and its state transitions — because an OAuth flow is a long-running, cancellable, multi-step job and the worker is the only thing allowed to touch the rc client.
- [ ] Commit checkpoint: `cloudsync: drive the rc config state machine`.

**Evidence:** The driver replays a captured OAuth state sequence to completion against the stub, with no provider name appearing in the code.
