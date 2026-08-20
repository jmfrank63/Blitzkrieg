# P00-M01 — rc JSON client

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Build and parse rc requests over HTTP to a running `rclone rcd`, including async jobs.

**Dependencies:** None. This is the first packet.

**Allowed files:** `Sources/src/CloudSync/rc.zig`, `Sources/src/CloudSync/rc_test.zig`, `build.zig`.

- [ ] Write `rc_test.zig` first, asserting against an in-process stub HTTP server (`std.net.Server` on an ephemeral port) that returns canned rc replies; the test must fail before `rc.zig` exists.
- [ ] Define `Endpoint = struct { host: []const u8, port: u16, user: []const u8, pass: []const u8 }` and `RcError = error{ Transport, Unauthorized, RcFailed, BadJson, Timeout }`.
- [ ] Implement `Client.init(allocator, io: std.Io, endpoint) !Client` holding a `std.http.Client`. The `io` field is mandatory in Zig 0.16 — `std.http.Client{ .allocator = a }` alone does not compile.
- [ ] Implement `Client.call(self, method: []const u8, params: std.json.Value) RcError!Reply` — POST to `http://host:port/<method>` with `Content-Type: application/json` and an HTTP Basic header built once in `init`.
- [ ] Parse failures into `RcFailure = struct { message: []const u8, status: u16 }` from the reply's `error` and `status` fields. rc failures arrive as HTTP 500 with a JSON body, not as a transport error, so they must not map to `Transport`.
- [ ] Implement `Client.callAsync(self, method, params) RcError!JobId`, injecting `"_async": true` and reading `jobid`.
- [ ] Implement `Client.jobStatus(self, JobId) RcError!JobStatus` returning `{ finished, success, error_text, output }`. Carry `output.output` through — for a bisync job that field holds the whole run log, and it is the only place the real reason for an abort appears.
- [ ] Cover in tests: a 200 reply, a 500 rc failure with the message extracted, a 401 mapping to `Unauthorized`, malformed JSON mapping to `BadJson`, and an async job polled from unfinished to finished.
- [ ] Add a `test-cloudsync-rc` step to `build.zig`, following the existing `blitz64-abi-test` step pattern.
- [ ] Commit checkpoint: `cloudsync: rc JSON client over HTTP`.

**Evidence:** `zig build test-cloudsync-rc -Dtarget=aarch64-macos -Dtest-mode=run` passes with no network access required.
