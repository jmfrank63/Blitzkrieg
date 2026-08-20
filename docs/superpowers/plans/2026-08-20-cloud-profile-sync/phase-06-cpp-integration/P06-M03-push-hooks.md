# P06-M03 — post-save and exit push

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Get local changes to the cloud at the two moments that matter, and bound the wait at exit.

**Dependencies:** P06-M02.

**Allowed files:** `Sources/src/Game/GameMain.cpp`, `Sources/src/Main/CloudSyncFacade.cpp`, `Sources/src/Main/iMainInternal.cpp`.

- [ ] Trigger a sync after a save completes, honouring `Cloud.Sync.OnSave` as declared in P05-M02, and coalesce bursts: three autosaves in a minute are one sync, not three.
- [ ] Never sync mid-mission. Queue the request and run it at the next safe point, so a network stall cannot touch frame pacing during play.
- [ ] At exit, honour `Cloud.Sync.OnExit` and wait with a bounded timeout and a visible indicator rather than blocking indefinitely; on timeout, leave the profile marked dirty for the next launch.
- [ ] Always call `NCloudSync::Shutdown()` on the exit path, including error and crash-handler paths, so the daemon dies with the game.
- [ ] Commit checkpoint: `cloudsync: push after saves and on exit`.

**Evidence:** A headless run performs a save, shows one coalesced sync, and exits with no surviving rclone process.
