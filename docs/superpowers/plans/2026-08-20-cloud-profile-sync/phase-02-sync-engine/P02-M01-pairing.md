# P02-M01 — pairing and resync bootstrap

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Establish the first pairing for a profile without letting it destroy the other side.

**Dependencies:** P01-M04.

**Allowed files:** `Sources/src/CloudSync/engine.zig`, `Sources/src/CloudSync/engine_test.zig`.

- [ ] Write the failing integration test against a live daemon and two local directories: first run pairs, second run does not resync.
- [ ] Include a pairing against a **completely empty remote** — no profile directory, no trash root, nothing — since that is what every player's first sync actually looks like.
- [ ] Implement `PairingState` persisted at `<stateRoot>/state/<profile>.json` — **outside Path1** — recording `paired: bool`, `last_success_unix: i64`, and the remote fingerprint used at pairing. Inside the profile it would sync to the other machine and misreport that machine's pairing.
- [ ] **Create the remote directories before pairing.** Issue `operations/mkdir` for Path2 and for the remote trash root first: bisync requires both base directories to exist, and a first pairing against a brand-new remote otherwise dies with `error reading source root directory: directory not found` followed by `Bisync critical error: directory not found` (verified, and reproducible even against a local Path2). This is the *normal* first-run state, not an edge case.
- [ ] Implement `Engine.pair(self, ctx) !void` issuing bisync with `resync: true` and `resyncMode: "newer"`.
- [ ] Before writing a sentinel, check the remote for one with `operations/stat`, and pass the result into `plan.ensureSentinel`. Seeding both sides independently aborts the resync as out of sync.
- [ ] Add the regression test that matters most here: local holds an older copy of a save, the remote a newer one, pairing runs, and **the newer copy survives on both sides**. Without `resyncMode` this test loses the newer save silently, which is exactly the failure it exists to catch.
- [ ] **Assert the loser is recoverable too, not merely that the winner is right.** The invariant is that a sync never destroys a save, and resync takes a different rclone path from ordinary conflict handling — the original measured failure produced no trash entry at all. Pass `backupDir1` and `backupDir2` on the pairing call and assert the overwritten version appears in the correct run-scoped trash.
- [ ] Test both directions, because the loser lands on its own side and the two cases exercise different code: with Path2 newer the local copy loses and appears in `backupDir1`; with Path1 newer the remote copy loses and appears in `backupDir2`. Both verified against rclone v1.75.0 — resync does honour the backup directories, so an empty trash here means the packet is wrong, not that rclone cannot do it.
- [ ] Refuse to auto-resync a profile already paired. A resync after real divergence overwrites one side, so recovery is an explicit player action routed through P02-M03.
- [ ] Treat a changed remote fingerprint (different bucket or endpoint) as a new pairing needing confirmation, not a resumable session.
- [ ] Commit checkpoint: `cloudsync: pair a profile without overwriting the newer side`.

**Evidence:** Integration output shows pairing in both directions preserving the newer save and placing the loser in the correct side's run-scoped trash, a pairing against a completely empty remote, and a second run whose parameters contain no `resync` key.
