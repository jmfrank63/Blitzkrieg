# P04-M04 — credentials dialog

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Let the player type an endpoint and a secret, which the options list cannot do.

**Dependencies:** P04-M03.

**Allowed files:** `Sources/src/GameTT/InterfaceCloudCredentials.h`, `Sources/src/GameTT/InterfaceCloudCredentials.cpp`, `Data/UI/CloudCredentials.xml`, `Data/Textes/UI/CloudCredentials`, `Sources/src/GameTT/InterfaceOptionsSettings.cpp`.

- [ ] Model the screen on the player-profile dialog, which is the existing precedent for an edit-box screen reached from a menu.
- [ ] Reuse the settings tab-bar 9-slice for buttons: the plain bar is at 590,957 and the gold-outline active bar at 590,888 in `ui\IntermissionTextures\\back-settings`, both 433x66, as `PlayerProfile.xml` already does.
- [ ] Provide fields for endpoint, bucket, region, access key, and secret, with the secret masked on screen.
- [ ] Write through `NCloudSync` to `cloud.credentials`; the dialog must never call `IOptionSystem::Set` for these values.
- [ ] Add a "Test connection" action issuing `operations/list` against the configured remote with a short timeout, reporting reachable or the classified failure from P02-M03.
- [ ] Reach the dialog from the Cloud tab. Since the options list renders only declared options, add the entry point as a screen button rather than a list row.
- [ ] Verify headlessly with `BK_AUTO_UI` `click=` actions, since raw platform mouse events are discarded without window focus.
- [ ] Commit checkpoint: `settings: cloud credentials dialog`.

**Evidence:** Headless capture shows the dialog, a masked secret, a successful round trip to `cloud.credentials`, and a connection test result.
