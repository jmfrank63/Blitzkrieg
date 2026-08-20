# P00-M03 — daemon supervisor

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Own the `rclone rcd` child: start it privately, know when it is ready, and never kill the wrong process.

**Dependencies:** P00-M02.

**Allowed files:** `Sources/src/CloudSync/daemon.zig`, `Sources/src/CloudSync/daemon_test.zig`.

- [ ] Extend the failing test to spawn a real rclone when one is discoverable and skip cleanly when not — a machine without rclone must not fail the suite.
- [ ] Implement `Daemon.spawn(allocator, io, opts) !Daemon` via `std.process.Child` with `--rc-addr 127.0.0.1:<port> --rc-serve=false --rc-user <u> --rc-pass <p> --log-file <path> --log-level INFO`.
- [ ] Derive the port by binding an ephemeral socket, reading the assignment, then closing it; derive user and password from `std.crypto.random`. Never reuse a fixed port or credential across launches.
- [ ] Point the child's `RCLONE_CONFIG` at a path inside the game directory so the daemon never reads or writes the player's own `rclone.conf`.
- [ ] Implement `Daemon.waitReady(self, timeout_ms) !void` polling `core/version`. Measured cold start on macOS arm64 is under 3 s; use a 15 s timeout and treat expiry as `.daemon_timeout` with the log tail attached.
- [ ] Implement `Daemon.shutdown(self) void` — terminate, wait, close the log, remove the pid file. Idempotent, and safe from an error path.
- [ ] **Make orphan reaping identity-safe.** A pid plus an executable name is not enough: a recycled pid belonging to some other rclone passes that test and gets killed. Persist `{ pid, process_start_time, nonce, port }` to `<gamedir>/cloudsync/daemon.json`, pass the nonce to the child (via `--rc-user`, which is already per-launch random, or an environment variable), and kill only when the pid is alive, its start time matches to the second, **and** the daemon answers `core/version` with that nonce's credentials.
- [ ] On Windows, prefer assigning the child to a job object with `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE` so the OS reaps it when the game dies, and keep the identity check as the fallback for a crash that outlives the job.
- [ ] If identity cannot be confirmed, **do not kill** — log the pid, leave it, and start a fresh daemon on a new port. A leaked process is a bug; killing an unrelated one is a defect.
- [ ] Assert in the test that no rclone survives `shutdown`, and that a forged pid file with a mismatched start time is refused rather than acted on.
- [ ] Commit checkpoint: `cloudsync: supervise the rclone rcd child with identity-checked reaping`.

**Evidence:** Test output shows a live `core/version` reply, zero surviving rclone processes after shutdown, and a forged pid file declined; the daemon log is attached.
