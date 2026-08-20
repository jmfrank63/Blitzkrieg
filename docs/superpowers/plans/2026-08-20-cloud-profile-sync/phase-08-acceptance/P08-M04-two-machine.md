# P08-M04 — two-machine convergence

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Test the thing the feature is actually for.

**Dependencies:** P08-M03.

**Allowed files:** `docs/superpowers/evidence/cloud-sync/p08-m04-convergence.md`.

- [ ] Pair the same profile on a Windows machine and a macOS machine against one remote.
- [ ] **Pair the second machine while it holds an older copy of a save the remote has newer**, and confirm the newer survives. This is the `resyncMode` case that silently destroyed a save in testing, and it only appears on a second machine.
- [ ] Save on A, sync, sync on B, and confirm the save arrives and loads.
- [ ] Delete a save on A, sync both, and confirm it is gone on B **and** recoverable from the trash. This is the case that killed the add-only design and it must be demonstrated, not argued.
- [ ] Edit the same save on both machines while offline, then sync both, and confirm the newer wins on both sides with the loser preserved as `.conflictN` on both.
- [ ] Back up config on A, restore it on B in merge mode, and confirm B keeps its own resolution and monitor.
- [ ] Run one deliberate mass-delete to confirm `too many deletes` surfaces as a prompt rather than mirroring the wipe.
- [ ] Confirm neither machine ever wrote its own sentinel over the other's.
- [ ] Human approval on both machines is required.
- [ ] Commit checkpoint: `cloudsync: two-machine convergence evidence`.

**Evidence:** Evidence records each scenario with before/after listings on both machines and the trash contents after the delete.
