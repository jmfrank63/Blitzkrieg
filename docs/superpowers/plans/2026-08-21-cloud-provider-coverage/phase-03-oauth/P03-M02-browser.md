# P03-M02 — browser launch and callback

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Get the player through the consent screen and back.

**Dependencies:** P03-M01.

**Allowed files:** `Sources/src/CloudSync/oauth.zig`, `Sources/src/Main/CloudSyncFacade.cpp`, `Sources/src/Main/CloudSyncFacade.h`, `Sources/src/GameTT/InterfaceCloudCredentials.cpp`.

- [ ] Open the consent URL through the existing platform launcher in `Sources/src/Platform/System.cpp` rather than adding a second way to open a URL.
- [ ] Poll `config/oauthstatus` for completion and offer `config/oauthstop` as a cancel, since both already exist in the rc API.
- [ ] Handle the fullscreen case: on macOS the game runs in its own Space, so sending the player to a browser and back needs a visible in-game state explaining what happened, not a silent wait.
- [ ] Bound the wait and make cancel always available. An OAuth flow the player abandoned must not leave the dialog stuck.
- [ ] Never log the redirect URL or any token; both carry credentials.
- [ ] Commit checkpoint: `cloudsync: browser-based OAuth consent`.

**Evidence:** A recorded run of one OAuth backend from consent to completion, with the cancel path exercised and no token in any log.
