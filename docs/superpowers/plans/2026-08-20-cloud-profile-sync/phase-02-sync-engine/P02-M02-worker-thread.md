# P02-M02 — worker thread and state machine

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Run every rc call off the main thread so a stalled socket can never cost a frame.

**Dependencies:** P02-M01.

**Allowed files:** `Sources/src/CloudSync/worker.zig`, `Sources/src/CloudSync/engine.zig`, `Sources/src/CloudSync/worker_test.zig`.

- [ ] Write the failing test first: point the engine at a server that accepts and never replies, then assert every `Engine.poll` call returns within a hard budget while the state eventually becomes `.failed` on the deadline.
- [ ] **`_async` is not enough.** It makes the rclone job asynchronous server-side; the initiating POST and every `job/status` POST still block the caller. Without a worker, `poll()` blocks the main loop on a socket, and the plan's central invariant has no mechanism.
- [ ] Implement `Worker` owning a thread built on the project's `std.Io.Threaded` service (`Sources/src/StreamIOZig/io_service.zig` is the precedent). The worker owns the `rc.Client` exclusively; no other thread may call it.
- [ ] Define `Snapshot = struct { state: State, outcome: Outcome, error_text: []const u8, progress: ?Progress }` published under a mutex, or as an atomically swapped pointer. `Engine.poll()` copies the snapshot and returns. **It performs no I/O of any kind.**
- [ ] Define `State = enum { idle, starting, pairing, syncing, done, failed }`. `Engine.begin(profile) !Handle` enqueues work and returns immediately — daemon spawn and readiness happen inside the worker and are observed through `poll`, never awaited in `begin`.
- [ ] Poll `job/status` from the worker on a wall-clock interval, not per frame; a 200 Hz menu must not produce 200 status calls a second.
- [ ] **Copy the rclone path out of the discovery cache before using it.** The worker is the reader the P00-M04 locking contract exists for: the UI thread can refresh that cache at any moment (P03-M01 does it on credentials save), and a borrowed pointer into it can be freed underneath a daemon spawn. Take an owned copy under the lock and release before spawning.
- [ ] Give the worker a cancellation path so `Shutdown` during an in-flight sync does not block exit past the bounded timeout.
- [ ] Assert in the test that the maximum observed `poll()` duration stays within one frame at 60 Hz even while the transport is hung.
- [ ] Commit checkpoint: `cloudsync: move every rc call onto a worker thread`.

**Evidence:** Test output records the maximum `poll()` duration against a hung server and the state reaching `.failed` on the deadline rather than hanging.
