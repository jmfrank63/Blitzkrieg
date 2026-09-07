# P05-M02 — Cloud option declarations

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Create the Cloud division by declaring options, with no new UI code.

**Dependencies:** P05-M01.

**Allowed files:** `Data/Configs/defconf.cfg`, `Data/Textes/Options/Cloud.name.txt`, `Data/Textes/Options/Cloud.tooltip.txt`, `Data/Textes/Options/Cloud.Enabled.name.txt`, `Data/Textes/Options/Cloud.Enabled.tooltip.txt`, `Data/Textes/Options/Cloud.Provider.name.txt`, `Data/Textes/Options/Cloud.Provider.tooltip.txt`, `Data/Textes/Options/Cloud.Sync.OnStartup.name.txt`, `Data/Textes/Options/Cloud.Sync.OnStartup.tooltip.txt`, `Data/Textes/Options/Cloud.Sync.OnSave.name.txt`, `Data/Textes/Options/Cloud.Sync.OnSave.tooltip.txt`, `Data/Textes/Options/Cloud.Sync.OnExit.name.txt`, `Data/Textes/Options/Cloud.Sync.OnExit.tooltip.txt`, `Data/Textes/Options/Cloud.Config.Backup.name.txt`, `Data/Textes/Options/Cloud.Config.Backup.tooltip.txt`.

- [ ] Add `Cloud.*` items to `Data/Configs/defconf.cfg` following the existing item shape: `EditorType`, `Flags`, `Order`, `Type`, `InstantApply`, `Action`, `ActionFill`, `Default`, `KeyName`.
- [ ] The division name is derived, not declared: `COptionSystem::GetDesc` takes the substring before the first dot (`OptionSystemInternal.cpp:168`), so every `Cloud.*` key lands in one `Cloud` division and the tab appears on its own.
- [ ] Declare **six** options, all `EditorType 3` (`EOET_CLICK_SWITCHES`, a droplist of string values): `Cloud.Enabled`, `Cloud.Provider` (`Off`/`S3`/`WebDAV`), `Cloud.Sync.OnStartup`, `Cloud.Sync.OnSave`, `Cloud.Sync.OnExit`, and `Cloud.Config.Backup`. Every lifecycle hook in phase 06 reads one of these; a hook with no option behind it cannot be configured.
- [ ] **Declare no free-text string option.** `COptionSystem::Set` truncates any string over 12 characters to 8 unless the option's action is `SetVideoMode` (`OptionSystemInternal.cpp:186-192`), which would silently destroy an endpoint, key, or secret. Those live in `cloud.credentials` (P03-M01).
- [ ] Default every option to the disabled value. Cloud sync must be off until a player turns it on, and a fresh install must add no startup latency.
- [ ] Use `OPTION_FLAG_GENERIC_OPTION` only. Cloud settings must not appear in the in-mission options screen, which filters on `OPTION_FLAG_CHANGE_IN_MISSION`.
- [ ] Add `Cloud.name.txt` and `Cloud.tooltip.txt` — `Create()` asserts on a missing `Textes\Options\<division>.name` — plus a `.name`/`.tooltip` pair per option.
- [ ] No change to `Data/UI/OptionsSettings.xml` is required or permitted: it already carries six tab buttons (10007-10012) and six lists (1000-1005) against today's four divisions.
- [ ] Commit checkpoint: `settings: add the Cloud options division`.

**Evidence:** Headless capture shows five tabs — GFX, GamePlay, Multiplayer, Sound, Cloud — with all six options listed, defaults disabled, and the in-mission options screen unchanged.
