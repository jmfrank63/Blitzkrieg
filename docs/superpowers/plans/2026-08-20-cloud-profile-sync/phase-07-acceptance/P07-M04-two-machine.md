# P07-M04 — two-machine convergence

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Test the thing the feature is actually for.

**Dependencies:** P07-M03.

**Allowed files:** `docs/superpowers/evidence/cloud-sync/p07-m04-convergence.md`.

- [ ] Pair the same profile on a Windows machine and a macOS machine against one remote.
- [ ] Save on A, sync, sync on B, and confirm the save arrives and loads.
- [ ] Delete a save on A, sync both, and confirm it is gone on B **and** recoverable from B's trash. This is the case that killed the add-only design and it must be demonstrated, not argued.
- [ ] Edit the same save on both machines while offline, then sync both, and confirm the newer wins on both sides with the loser preserved as `.conflictN` on both.
- [ ] Back up config on A, restore it on B in merge mode, and confirm B keeps its own resolution and monitor.
- [ ] Run one deliberate mass-delete to confirm `too many deletes` fires as a prompt rather than mirroring the wipe.
- [ ] Human approval on both machines is required.
- [ ] Commit checkpoint: `cloudsync: two-machine convergence evidence`.

**Evidence:** Evidence records each scenario with before/after listings on both machines and the trash contents after the delete.
