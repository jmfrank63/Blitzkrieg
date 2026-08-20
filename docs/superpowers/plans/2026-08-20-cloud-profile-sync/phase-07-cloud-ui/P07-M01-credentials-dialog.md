# P07-M01 — credentials dialog

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Let the player type an endpoint and a secret, which the options list cannot do.

**Dependencies:** P06-M04 and P03-M04.

**Allowed files:** `Sources/src/GameTT/InterfaceCloudCredentials.h`, `Sources/src/GameTT/InterfaceCloudCredentials.cpp`, `Data/UI/CloudCredentials.xml`, `Data/Textes/UI/CloudCredentials`, `Sources/src/GameTT/InterfaceOptionsSettings.cpp`.

- [ ] Model the screen on the player-profile dialog, the existing precedent for an edit-box screen reached from a menu.
- [ ] Reuse the settings tab-bar 9-slice for buttons: the plain bar at 590,957 and the gold-outline active bar at 590,888 in `ui\IntermissionTextures\back-settings`, both 433x66, as `PlayerProfile.xml` already does.
- [ ] Provide fields for endpoint, bucket, region, access key, and secret, with the secret masked.
- [ ] Populate from `NCloudSync` credentials load, which deliberately does not return the secret — show a masked placeholder when `has_secret` is set, and send a secret only when the player types a new one.
- [ ] Write through `NCloudSync`; the dialog must never call `IOptionSystem::Set` for these values, which would truncate anything over 12 characters to 8.
- [ ] Wire "Test connection" to the pollable `NCloudSync` connection test, showing the classified outcome rather than a bare failure, and never blocking the UI thread while it runs.
- [ ] Reach the dialog from the Cloud tab as a screen button, since the options list renders only declared options and none of these are options.
- [ ] Verify headlessly with `BK_AUTO_UI` `click=` actions, since raw platform mouse events are discarded without window focus.
- [ ] Commit checkpoint: `settings: cloud credentials dialog`.

**Evidence:** Headless capture shows the dialog, a masked secret, a round trip to `cloud.credentials`, and each connection-test outcome.
