# P00-M02 — available out of the box

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Prove the shipped game finds its own rclone with nothing installed on the machine.

**Dependencies:** P00-M01.

**Allowed files:** `Sources/src/CloudSync/daemon_test.zig`, `tools/zig/cloudsync_abi_test.cpp`.

- [ ] Write the failing test first: a staged layout plus an **empty** `PATH` must still resolve to the bundled binary.
- [ ] Assert through `discover` and through `bk_cloudsync_available`, since the second is what the settings screen actually reads.
- [ ] Assert the version gate passes against the bundled copy — a staged binary older than `MIN_RCLONE` must fail loudly here rather than at first sync.
- [ ] Assert an explicit `rclone_path` still overrides the bundled copy. A player who points at their own build must keep winning over ours.
- [ ] Do not weaken the existing skip-when-absent behaviour: the suite must still pass on a machine with neither a bundled nor an installed rclone.
- [ ] Commit checkpoint: `cloudsync: find the bundled rclone with an empty PATH`.

**Evidence:** Test output shows resolution succeeding with `PATH` emptied, the override still winning, and the suite still passing with no rclone anywhere.
