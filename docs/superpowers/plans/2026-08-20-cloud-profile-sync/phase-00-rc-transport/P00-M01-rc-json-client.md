# P00-M01 — rc JSON client

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Build and parse rc requests over HTTP, with a deadline on every call, including async jobs.

**Dependencies:** None. This is the first packet.

**Allowed files:** `Sources/src/CloudSync/rc.zig`, `Sources/src/CloudSync/rc_test.zig`, `build.zig`.

- [ ] Write `rc_test.zig` first against an in-process stub HTTP server (`std.net.Server` on an ephemeral port) returning canned rc replies; the test must fail before `rc.zig` exists.
- [ ] Define `Endpoint = struct { host, port, user, pass }` and `RcError = error{ Transport, Unauthorized, RcFailed, BadJson, Timeout }`.
- [ ] Implement `Client.init(allocator, io: std.Io, endpoint) !Client` holding a `std.http.Client`. The `io` field is mandatory in Zig 0.16 — `std.http.Client{ .allocator = a }` alone does not compile.
- [ ] Implement `Client.call(self, method, params) RcError!Reply` — POST to `http://host:port/<method>`, `Content-Type: application/json`, HTTP Basic header built once in `init`.
- [ ] **Give every call a deadline.** Set `Deadline = struct { connect_ms, read_ms }` on the client and enforce it with socket-level timeouts, mapping expiry to `RcError.Timeout`. A wedged daemon must fail a call, not hold it forever — and the caller cannot recover a hang it is never told about.
- [ ] Add the hung-server test: a `std.net.Server` that accepts the connection and never writes. `Client.call` must return `Timeout` within the configured deadline. This test is the contract; without it the non-blocking invariant has no mechanism behind it.
- [ ] Document at the top of `rc.zig` that **every method here blocks the calling thread**. `_async` makes the rclone job asynchronous server-side; the initiating POST and each `job/status` POST are ordinary synchronous requests. Callers on the main thread are a bug, and P02-M02 provides the worker.
- [ ] Parse failures into `RcFailure = struct { message, status }` from the reply `error` and `status` fields. rc failures arrive as HTTP 500 with a JSON body, so they must not map to `Transport`.
- [ ] Implement `Client.callAsync(self, method, params) RcError!JobId` injecting `"_async": true`, and `Client.jobStatus(self, JobId) RcError!JobStatus` returning `{ finished, success, error_text, output }`. Carry `output.output` through — for bisync that field holds the run log and is the only place an abort explains itself.
- [ ] Cover: a 200 reply, a 500 rc failure with the message extracted, 401 to `Unauthorized`, malformed JSON to `BadJson`, an async job polled to finished, and the hung server to `Timeout`.
- [ ] Add a `test-cloudsync-rc` step to `build.zig` following the `blitz64-abi-test` pattern.
- [ ] Commit checkpoint: `cloudsync: rc JSON client with per-call deadlines`.

**Evidence:** `zig build test-cloudsync-rc -Dtarget=aarch64-macos -Dtest-mode=run` passes, including the hung-server case returning `Timeout` inside the deadline.
