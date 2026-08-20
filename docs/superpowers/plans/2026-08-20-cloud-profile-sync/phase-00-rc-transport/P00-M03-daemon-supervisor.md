# P00-M03 — daemon supervisor

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Own the `rclone rcd` child: start it privately, know when it is ready, and never leave one behind.

**Dependencies:** P00-M02.

**Allowed files:** `Sources/src/CloudSync/daemon.zig`, `Sources/src/CloudSync/daemon_test.zig`.

- [ ] Extend the failing test to spawn a real rclone when one is discoverable, and skip cleanly when not — a machine without rclone must not fail the suite.
- [ ] Implement `Daemon.spawn(allocator, io, opts) !Daemon` via `std.process.Child` with `--rc-addr 127.0.0.1:<port> --rc-serve=false --rc-user <u> --rc-pass <p> --log-file <path> --log-level INFO`.
- [ ] Generate the port by binding an ephemeral socket, reading the assigned port, then closing it; generate user and password from `std.crypto.random`. Never reuse a fixed port or credential across launches.
- [ ] Point the child's `RCLONE_CONFIG` at a path inside the profile area, so the daemon never reads or writes the player's own `rclone.conf`.
- [ ] Implement `Daemon.waitReady(self, timeout_ms) !void` polling `core/version` through the P00-M01 client. Measured startup on macOS arm64 is under 3 s; use a 15 s timeout and treat expiry as `.daemon_timeout` with the log tail attached.
- [ ] Implement `Daemon.shutdown(self) void` — terminate, wait, close the log, remove the pid file. It must be idempotent and safe to call from an error path.
- [ ] Record child pid and port in `profiles/.cloudsync.pid` on spawn. Implement `reapOrphan(allocator)` which at startup kills a recorded pid only if it is alive **and** its executable name is rclone, then removes the file. A recycled pid must never be killed.
- [ ] Assert in the test that no rclone process survives `shutdown`.
- [ ] Commit checkpoint: `cloudsync: supervise the rclone rcd child`.

**Evidence:** Test output shows a live `core/version` reply and then zero surviving rclone processes; the daemon log is attached to the evidence record.
