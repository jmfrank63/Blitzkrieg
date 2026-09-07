# P03-M01 — rc config state machine

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Drive rclone's interactive config from Zig, without a terminal.

**Dependencies:** P02-M04 — the state machine surfaces prompts the generic form renders, and its completion path runs the same connection test, so it needs validation in place, not merely rendering.

**Allowed files:** `Sources/src/CloudSync/oauth.zig`, `Sources/src/CloudSync/oauth_test.zig`, `Sources/src/CloudSync/worker.zig`, `Sources/src/CloudSync/engine.zig`, `Sources/src/CloudSync/form.zig`, `Sources/src/CloudSync/form_test.zig`, `build.zig`.

- [ ] Write the failing test against a stub rc server replaying a captured state sequence before writing the driver.
- [ ] Implement the `config/create` state loop. **Request options are lowercase; responses are capitalised. They are not the same spelling.** rclone's own documentation lists the request `opt` keys as `state`, `result`, `continue`, `nonInteractive`, `obscure`, `noObscure`, `noOutput` and `all`, while the reply carries `State`, `Option` and `Error`. Posting `State`/`Result` is silently ignored and the loop never advances — a failure that looks like a hang, not a rejection.
- [ ] Send `nonInteractive`, and drive the machine with `continue`, `state` and `result`. There is no terminal to fall back to.
- [ ] **Resend the complete envelope on every continuation** — `name`, `type` and the full `parameters` map, not just the continuation flags. rclone rebuilds the remote's configuration from the request each time; a continuation carrying only `state` and `result` loses the parameters and the flow fails or configures something incomplete.
- [ ] Make the stub assert the whole repeated envelope, so a continuation that drops `parameters` fails the test rather than passing and breaking against a real service.
- [ ] Treat the state machine as opaque data. Do not interpret provider-specific states; the loop must be able to carry a backend added after this code was written.
- [ ] Surface an intermediate step that needs a value as a generic prompt built the same way P02-M01 builds a field, so the dialog renders it without new code. `form.zig` is in this allowlist for that conversion — the reply's `Option` block has the same shape as a catalogue option, so the helper is shared rather than duplicated.
- [ ] Bound every step with the existing deadline mechanism, and run it on the worker like every other rc call. **That means owning the worker changes here** — a new job kind and its state transitions — because an OAuth flow is a long-running, cancellable, multi-step job and the worker is the only thing allowed to touch the rc client.
- [ ] Commit checkpoint: `cloudsync: drive the rc config state machine`.

**Evidence:** The driver replays a captured OAuth state sequence to completion against the stub, with no provider name appearing in the code.
