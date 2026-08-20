# P03-M02 — startup pull

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Sync before the profile config is read, without making the player wait on a network to reach the menu.

**Dependencies:** P03-M01.

**Allowed files:** `Sources/src/Game/GameMain.cpp`, `Sources/src/Main/CloudSyncFacade.cpp`, `Sources/src/Main/CloudSyncFacade.h`.

- [ ] Add the hook after `NProfile` bootstrap resolves the active profile but before the profile config is read — a pull that lands after the config load has no effect on this session.
- [ ] `Begin()` at that point, then let the main loop poll. Never spin on `Poll()` at startup: on a slow link that is an unbounded stall before the first frame.
- [ ] Reap an orphan daemon from a previous crashed run before spawning, through the P00-M03 path.
- [ ] When `Available()` is false, or the profile has cloud sync disabled, take no action at all and add no startup latency.
- [ ] Respect `Cloud.Sync.OnStartup`; the option is read from the profile config, which is loaded after the hook fires, so read it from the previous session's cached value and treat a missing value as disabled.
- [ ] Verify headlessly with `BK_AUTO_UI`, scheduling `40:var=notransition=1` first so the curtain transition does not swallow injected messages.
- [ ] Commit checkpoint: `cloudsync: pull the profile at startup`.

**Evidence:** A headless launch shows the sync starting before config load and the main menu appearing without waiting for it; frame timing across the sync is recorded.
