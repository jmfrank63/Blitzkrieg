# P02-M02 — async job polling and state machine

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Run a sync to completion from the game loop without ever blocking a frame.

**Dependencies:** P02-M01.

**Allowed files:** `Sources/src/CloudSync/engine.zig`, `Sources/src/CloudSync/engine_test.zig`.

- [ ] Write the failing test asserting that no engine call blocks longer than a set budget and that a full sync still reaches `.done`.
- [ ] Define `State = enum { idle, starting, pairing, syncing, done, failed }` and implement `Engine.begin(profile) !Handle` plus `Engine.poll(handle) State`.
- [ ] `begin` must return before the daemon is ready: spawn and readiness happen inside the state machine, observed through `poll`, not awaited inside `begin`.
- [ ] Poll the rc job with `job/status` at a fixed interval measured in wall-clock time, not per frame — a 200 Hz menu must not issue 200 status calls a second.
- [ ] On `finished: true`, read `success`; on failure keep `output.output` — the run log — for P02-M03 to classify. Do not discard it as noise.
- [ ] Update `last_success_unix` in `PairingState` only on a clean finish.
- [ ] Commit checkpoint: `cloudsync: non-blocking sync state machine`.

**Evidence:** Test output shows a completed sync with the maximum single-call duration recorded and well inside one frame.
