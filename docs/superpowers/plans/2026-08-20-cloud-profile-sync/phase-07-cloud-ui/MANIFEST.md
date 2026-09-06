# Phase 07 — Cloud UI

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Give the player the two screens the feature cannot be used without: where credentials are entered, and where backups are restored.

| Packet | Depends on | Owns |
|---|---|---|
| P07-M01 | P06-M04, P03-M04 | credentials dialog and connection test |
| P07-M02 | M01, P04-M02 | backup browser |
| P07-M03 | M02, P04-M04 | restore confirmation and undo |

Exit: a player can enter credentials, test the connection, browse backups, restore one, and undo it, without editing a file by hand.

P07-M01 Windows checkpoint: measured end to end on the release build against
a live MinIO — the dialog opened from the Cloud tab, every field typed
through the harness, a byte-exact round trip to `cloud.credentials`
including a 160-character rclone override, the next launch pairing from
dialog-entered credentials alone, and probe outcomes captured for
connection_ok, auth_failed, remote_missing and remote_unreachable
(evidence/cloud-sync/credentials-dialog/, nine screenshots). All suites
green offline and live; facade cross-compiles pass. Commit `641270f5e`.

Carried forward from P07-M01:

- **Two latent defects fixed, found by wiring the first real consumer.**
  (1) The facade returned RAW library handles from `TestConnection`,
  `ListBackups`, `RestoreBackup` and `UndoRestore` while
  Poll/Error/Cancel/Release translate through the facade job table — a
  probe handle could never be observed ("no such cloud sync job"). Every
  handle the facade returns is now a facade slot (`WrapLibraryHandle`), and
  `BackupEntry` translates its handle. P07-M02/M03 inherit working handles.
  (2) The worker applied `cloud.credentials` to the daemon once per
  session; a probe after an edit tested the PREVIOUS credentials.
  `applyCredentials` now runs at every job start. The daemon *binary*
  still belongs to the session — an rclone_path change takes effect on the
  next daemon.
- **GameTT reaches NCloudSync by compiling the facade itself**
  (`..\Main\CloudSyncFacade.cpp` + `..\Platform\CloudSyncLoader.cpp` in
  GameTT.vcxproj). Both facade copies share one CloudSync.dll instance —
  handle tables are per-copy, worker and daemon are shared, and the
  exe/dialog jobs already serialize through the worker's one-job-at-a-time
  discipline. New interface+command ids live in `iMission.h` (+259/+260),
  registered in `MissionObjectFactory.cpp`.
- The dialog screen is NON-modal (no ModalFlag child), so its container's
  children pick normally in document order; the exit-confirm ModalFlag
  trick is specific to the main menu. Deserialized children keep document
  order — content first, chrome last, which is also visual top-to-bottom.
- `CUIEditBox` truths: `TextScroll="1"` is required for any value longer
  than the box (without it typing SILENTLY stops at the visible width);
  `GetWindowText` returned only the visible scrolled slice — now overridden
  to return the full text (chat entry benefited too); `MaxLength` bounds
  length, the settings-store 12-char limit never applies here.
- The harness gained `text=<utf8>` (a synthetic platform textInput event
  through `IInput::ConsumePlatformEvent`), typing into the FOCUSED edit box
  — click the box first. `event.text` is 64 bytes: split longer values
  into consecutive `text=` actions. Commas are the schedule separator and
  cannot appear in values; colons are fine (only the first is consumed).
- The secret never exists in the UI: the box shows mask characters over a
  dialog-held wide string, rebuilt on TEXT_CHANGED by mapping leading and
  trailing mask runs onto the kept prefix/suffix of the real value. A
  stored secret displays as an 8-star placeholder; saving without touching
  preserves it (`mergeOmittedSecret`); "Forget secret" is the explicit
  clear. Save composes JSON by hand (escaping included) and parses the
  facade documents with a flat key scan — every key name is unique per
  document.
- The settings screen's sixth tab slot (45,630) hosts the "Cloud
  storage..." button — visible only while the Cloud tab is active
  (`nCloudDivision`), translated by OptionsSettings.lua (range extended to
  10013). A mod filling all six tab slots would overlap it.
- Probe outcomes reachable from the dialog and captured: connection_ok,
  auth_failed (cleared secret), remote_missing (bad bucket),
  remote_unreachable (dead endpoint). The remaining vocabulary arrives
  through sync runs, not probes.
- `-mode=1024x768` + click coordinates: the dialog is centered 736x610, so
  screen = dialog + (144,79). Tab CLOUD center (260,562), dialog button
  (260,660), provider toggle (334,167), test (334,573), forget (690,573),
  V (440,634), X (578,634), row N edit center (500, 118+36(N-1)+15+79).

P07-M02 Windows checkpoint: measured on the release build against a live
MinIO — the loading line, the empty-bucket explanation, and a listing
across two hosts (this machine's real snapshots plus a staged
"old-desktop" history), grouped by host and newest first
(evidence/cloud-sync/backup-browser/). All suites green offline and live;
facade cross-compiles pass. Commit `4561186fa`.

Carried forward from P07-M02:

- **The list control REQUIRES one header child per column** (ids 10, 11,
  12, ...): `CUIList`'s deserialize resolves `GetChildByID(10 + i)` for
  every ColumnProps entry, asserts in debug, and in release stores the
  null and crashes inside the screen's first `Reposition`
  (`headers.subItems[i].pElement->SetWindowPlacement`). Every multi-column
  list in the data obeys this; a new list must too.
- The Cloud tab's sixth-slot buttons are now two half-width bars:
  "Storage..." (10013, x 45..257) and "Backups..." (10014, x 266..478),
  centers (151,663) and (372,663). OptionsSettings.lua's translated range
  is 10001..10014.
- The browser fetches on open (`ListBackups` + facade poll), keeps
  `entryIDs` per row (user data indexes it) for P07-M03's restore, maps
  failures through the same classified-tag lookup, and explains the empty
  case. The staged second host was written with plain `rclone copyto`
  onto `config-backups/<profile>/<host>/<stamp>.cfg` — a fine fixture
  recipe for restores too.
- An empty BUCKET lists as empty (the engine's missing-backup-root case);
  a missing bucket fails classified. The X icon button's element id IS
  IMC_CANCEL (10001) — one switch case, not two.
- A warm daemon session lists in well under 300 ms — a loading-state
  screenshot needs its shot within ~4 frames of the opening click.

P07-M03 Windows checkpoint: verified headlessly across REAL restarts
against a live MinIO — a merge restore replaced `Cloud.Sync.OnSave` with
the backup's value while a local `GFX.DensityCoeff` change survived; the
undo of an applied restore staged the previous settings back and, after
another relaunch, left `config.cfg` byte-identical (SHA-256) to the
pre-restore original. Screenshots cover browse, the merge confirmation,
the full-restore warning, staged-with-cancel-pending, undo-applied
offered, and the undo staged back
(evidence/cloud-sync/restore-confirm/). All suites green offline and
live; facade cross-compiles pass. Phase 07 closed. Commit `1f2776d15`.

Carried forward from P07-M03:

- The restore flow is a state machine on the browser screen (browse /
  confirm-merge / confirm-full), buttons 10030/10031 relabeled per state,
  the explanation label doing the plain-terms work. Escape inside a
  confirmation goes BACK, never past the explicit step.
- Undo is named for what `UndoAvailability` reports and the settle message
  matches what the undo DID: a cancelled pending stage "is discarded", a
  reversed applied restore "is staged back - the next launch restores
  them". After the undo stages back, availability correctly reads
  CANCELLABLE again (the undo itself is a pending stage).
- Acceptance tooling traps, both stepped in during this packet:
  (1) reading config.cfg with a lazy regex crossing item boundaries
  reports the WRONG item's value — anchor at the `<item` containing the
  `<KeyName>` and take the FIRST `<Var>` inside it (scratchpad
  `read-cfg.ps1` does it right); the P06-M02 lesson, relearned in
  PowerShell. (2) `BK_AUTO_UI` actions fire on EXACT frame numbers and a
  skipped frame silently drops one — and with `Cloud.Config.Backup` ON,
  every synced launch snapshots the CURRENT config, so "restore the
  newest row" restores what you already have; either stop startup syncs
  (`Cloud.Sync.OnStartup=OFF`) before staging divergence or select an
  older row.
- `set=GFX.Blood` appears not to stick headlessly (its live SetBlood
  action path); `GFX.DensityCoeff` (InstantApply="0") is a reliable GFX
  probe.
