# P07-M01 — credentials dialog

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Let the player type an endpoint and a secret, which the options list cannot do.

**Dependencies:** P06-M04 and P03-M04.

**Allowed files:** `Sources/src/GameTT/InterfaceCloudCredentials.h`, `Sources/src/GameTT/InterfaceCloudCredentials.cpp`, `Data/UI/CloudCredentials.xml`, `Data/Textes/UI/CloudCredentials`, `Sources/src/GameTT/InterfaceOptionsSettings.cpp`.

- [ ] Model the screen on the player-profile dialog, the existing precedent for an edit-box screen reached from a menu.
- [ ] Reuse the settings tab-bar 9-slice for buttons: the plain bar at 590,957 and the gold-outline active bar at 590,888 in `ui\IntermissionTextures\back-settings`, both 433x66, as `PlayerProfile.xml` already does.
- [ ] **Show fields per protocol, not one union of everything.** S3 asks for vendor, endpoint, bucket, region, access key, and secret; WebDAV asks for URL, vendor, user, and password. Switching `Cloud.Provider` swaps the field set — presenting a WebDAV user with a bucket field is how a configuration screen teaches people the software does not know what it is doing.
- [ ] Mask the secret and password fields.
- [ ] **Add the rclone binary path field.** The feature is unavailable without the binary, bundling and auto-download are both out of scope, and `Credentials.rclone_path` already exists to hold it — so this field is the only way a player with rclone installed somewhere unusual can turn the feature on at all. Read `NCloudSync::DiscoveryStatus()` (P00-M04, wrapped in P06-M01) to show the discovered path and version when one was found, the typed rejection reason when it was not (`not_found`, `too_old`, `not_executable`), and let the player type or browse to an override.
- [ ] Re-run discovery when the path changes and report the result immediately, rather than leaving the player to infer it from a failed sync. Saving is what triggers it — `creds_save` invalidates the cache (P03-M01) — so the dialog re-reads `NCloudSync::DiscoveryStatus()` after a successful save and updates the shown path, version, and reason in place.
- [ ] Populate from `NCloudSync` credentials load, which deliberately does not return the secret — show a masked placeholder when `has_secret` is set, and send a secret only when the player types a new one. Saving without touching that field preserves the stored secret (P03-M01); offer clearing as an explicit action, never as a consequence of an empty box.
- [ ] Write through `NCloudSync`; the dialog must never call `IOptionSystem::Set` for these values, which would truncate anything over 12 characters to 8.
- [ ] Wire "Test connection" to the pollable `NCloudSync` connection test, showing the classified outcome rather than a bare failure, and never blocking the UI thread while it runs.
- [ ] Reach the dialog from the Cloud tab as a screen button, since the options list renders only declared options and none of these are options.
- [ ] Verify headlessly with `BK_AUTO_UI` `click=` actions, since raw platform mouse events are discarded without window focus.
- [ ] Commit checkpoint: `settings: cloud credentials dialog`.

**Evidence:** Headless capture shows the dialog, a masked secret, a round trip to `cloud.credentials`, each connection-test outcome, and an explicit rclone path overriding discovery.
