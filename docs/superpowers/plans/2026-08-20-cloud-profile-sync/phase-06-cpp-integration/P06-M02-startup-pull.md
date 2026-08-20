# P06-M02 — startup pull

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Sync before the profile config is read, without making the player wait on a network to reach the menu.

**Dependencies:** P06-M01.

**Allowed files:** `Sources/src/Game/GameMain.cpp`, `Sources/src/Main/CloudSyncFacade.cpp`, `Sources/src/Main/CloudSyncFacade.h`.

- [ ] Add the hook after `NProfile` bootstrap resolves the active profile but before the profile config is read — a pull landing after the config load has no effect on this session.
- [ ] **Settle the bootstrap ordering problem explicitly.** `Cloud.Sync.OnStartup` lives in the profile config, which is not loaded yet when the hook must fire. Do not invent a cached value: read the option file directly with a minimal parse for the `Cloud.*` keys only, before the full option system initialises. Record in the code why the normal path cannot be used.
- [ ] Treat a missing or unparsable value as disabled. A first launch, a corrupt config, or a profile that predates the feature must all mean "do nothing".
- [ ] `Begin()` at that point and let the main loop poll. Never spin on `Poll()` at startup: on a slow link that is an unbounded stall before the first frame.
- [ ] **Apply any staged config restore in this same early window**, before `OptionSystem::Init()` reads the file: move `config.cfg.pending-restore` over `config.cfg` and clear the marker. This is the only point where the option system has not yet loaded and the shutdown rewrite cannot have happened, which is why P04-M03 stages rather than writes.
- [ ] Reap an orphan daemon from a previous crashed run before spawning, through the identity-checked P00-M03 path.
- [ ] When `Available()` is false or the profile has sync disabled, take no action and add no startup latency.
- [ ] Verify headlessly with `BK_AUTO_UI`, scheduling `40:var=notransition=1` first so the curtain transition does not swallow injected messages.
- [ ] Commit checkpoint: `cloudsync: pull the profile at startup`.

**Evidence:** A headless launch shows the sync starting before config load and the menu appearing without waiting for it; frame timing across the sync is recorded.
