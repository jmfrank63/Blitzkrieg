# Cloud Provider Row

The Cloud tab's switch is the Provider row. This document restructures the
Cloud settings tab so that one row both turns cloud sync on and names the
storage service, and moves the two cloud screens into the tab's own box.

Amends `docs/superpowers/specs/2026-08-21-cloud-provider-coverage-design.md`:
its "`Cloud.Provider` cannot carry the backend name" consequence no longer
holds, and the chooser it moved into the credentials dialog moves back out.
Everything else there — the catalogue, the generic form, the credentials
document — stands.

## The problem, as shipped

The Cloud tab shows a "Cloud sync" switch and a "Provider" switch, both
ON/OFF, and nothing reads the second one. Every lifecycle gate checks
`Cloud.Enabled` alone (`GameMain.cpp`, the startup raw scan and the two live
checks); `Cloud.Provider` is declared in `defconf.cfg` and consumed nowhere.
So "Cloud sync ON, Provider OFF" syncs, and the row that looks like the
important one is decoration.

That row was reduced to ON/OFF because the legacy option store truncated
any string over 12 characters to 8 (`StreamIO/OptionSystemInternal.cpp:187`)
and `googlecloudstorage` would not survive. The legacy store is no longer
compiled; the live Zig store (`StreamIOZig/options.zig`, `set`) stores any
length. The reason the selection left the options list is gone.

Two smaller findings. "Back up settings" is a legitimate toggle — after each
clean sync the worker snapshots the profile's `config.cfg` to
`<remote>:config-backups/<profile>/<host>/` — but the row name does not say
so. And the Storage… and Backups… buttons sit in the left column's spare tab
slot rather than with the settings they belong to.

## Decision

**`Cloud.Provider` is the switch and the selector. The credentials document
stays the configuration.** Its value is `Off` or an rclone backend id;
`Cloud.Enabled` is deleted. The credentials dialog takes its backend from the
row instead of its own chooser, and a sync runs only when the saved
credentials' backend equals the row.

Two stored values name the backend — the option and the credentials
document — and the rule between them is asymmetric on purpose. The row is
cheap to change and is changed casually (an arrow key steps it). The
document is written only by a deliberate save in the dialog, and a backend
change there drops the previous backend's options wholesale by the existing
isolation rule. Backing the row directly by the document was considered and
rejected: stepping through 58 candidates would rewrite the document 58
times, and overshooting `s3` by one would destroy the S3 setup.

A mismatch between the two is therefore a normal, visible state — "chosen
but not set up" — and never a sync against the wrong service.

## The option

- `Cloud.Provider`: `EditorType 3`, instant apply, default `Off`. Values are
  `Off` first, then every catalogue candidate (the destination list the
  facade already filters, `bk_cloudsync_catalogue_destinations`) sorted by
  id, case-insensitive. The row's current value is always in the list, and
  so is the saved credentials' backend when one exists — both merged into
  sorted position even when the running rclone's catalogue lacks them.
  Values are rclone's ids as the
  dialog's "Service:" line shows them today (`s3`, `webdav`,
  `google cloud storage` — spaces are rclone's own); descriptions run to
  534 characters and are not row material.
- `Cloud.Enabled` and its two texts are removed. The feature is unreleased;
  there is no migration. A profile config that still carries the key must
  not surface as a stray row: the plan verifies this against an existing
  profile and, if the store keeps unknown keys visible, removes it at load.
- `Cloud.Config.Backup` keeps its semantics; its name becomes "Back up
  settings after sync". The Provider tooltip becomes "Off, or the storage
  service to sync this profile with. Set it up under Config…".

**The list must always contain the stored value.** On OK the settings screen
applies every row of every division, and a droplist whose stored value is
absent from its list resolves to entry 0 (`UIOptions.h`, `COptionSelection`)
— which here is `Off`. A Provider row filled from an empty catalogue would
turn cloud sync off on any OK press. Hence the fallback list is
`Off` + the stored value, never `Off` alone.

**Who fills the row.** Droplist values come from the option bridge in the
streamio library, which links SDL and the platform layer and cannot reach
the cloud facade. The settings screen can. `COptionsListWrapper` gains a
per-row value override — a name-to-values map consulted in `InitList` for
`EOET_CLICK_SWITCHES` rows before `GetDropValues` — and the settings screen
supplies the Provider row's list from the facade. The bridge's stale
`GetCloudProvider` fill (`Off`/`S3`/`WebDAV`) becomes the safe fallback:
`Off` plus the current value.

**Catalogue.** Activating the Cloud tab calls `EnsureCatalogue` (a deliberate
player action, the same rule under which the dialog spawns the daemon) and
polls it the way the dialog does. Until the fetch lands the row offers the
fallback list; when it lands, the Cloud division is rebuilt with the full
list and the current value kept.

## The Cloud tab

Rows, in order: **Provider**; then, only when Provider is not `Off`: Sync on
startup, Sync after saving, Sync on exit, Back up settings after sync.

The screen watches the Provider value after every message it processes
(keyboard left/right and the row's click controls both commit through the
instant-apply path, so the option system holds the new value at once). A
change rebuilds the Cloud division: a new `COptionsListWrapper` over the
filtered descriptor list, selection returned to the Provider row. Hidden
rows keep their stored values; setting a provider again shows them as they
were.

**Config…** (the credentials dialog, renamed from Storage…) and **Backups…**
move to the bottom-right inside the Cloud box, side by side, visible only
when Provider is not `Off`. Starting geometry in the screen's 1024×768
space, to be adjusted against a capture: the list box is at (527,130)
462×511 with a 28-wide scrollbar, so the buttons (212×66 each, 4 apart) sit
at x=529 and x=745, y=569. The left-column slot they occupy today is freed.

Risk: the buttons overlay the list control's empty lower area, and the list
must not swallow their clicks. The plan verifies with the headless click
harness; the fallback is declaring the buttons as children of the list
element in `OptionsSettings.xml` and looking them up through the list.

## The Config… dialog

- Opens with the backend fixed to the Provider row's value. Element 10020,
  today the destination chooser, is a disabled "Service: `<id>`" label while
  the catalogue is ready; while the catalogue is missing it keeps its retry
  role unchanged.
- `OnCatalogueReady` no longer picks `destinations[0]`; the backend is
  given. Stored options and the stored root load only when the stored
  backend equals the row — the condition `RebuildForm` already keys on.
- A backend the running catalogue lacks lands in the existing empty-form,
  cannot-save state with its status text; the player steps the row to a
  backend that exists.
- Save writes `backend` = the row's value, which is what the save JSON's
  `backend` field carries today. After a save the two stored values agree
  and sync is possible.

## Sync gates and the indicator

- New facade call `NCloudSync::CredentialsBackend(out, cap)` over a new
  export `bk_cloudsync_creds_backend`, a copy of
  `bk_cloudsync_creds_fingerprint` returning the loaded document's
  `backend`; −1 when no credentials are saved.
- The three `Cloud.Enabled` gates in `GameMain.cpp` become
  `Cloud.Provider != Off`; the startup raw scanner `CloudOptionIsOn`
  generalises to `CloudOptionValue`, same first-`<Var>`-after-item rule.
  Each gate additionally requires `CredentialsBackend() == Provider`.
  Neither check loads the cloud library when Provider is `Off`, so a fresh
  install still pays nothing at startup.
- When Provider is set but the credentials are absent or for another
  backend, no job starts. The main loop publishes `CloudSync.State =
  STATE_FAILED`, `CloudSync.Error = "unconfigured"` — at startup, and again
  when the settings screen closes (OK or Cancel): the screen raises a
  `CloudSync.Recheck`
  global var that the loop consumes, the mechanism `SkipToOffline` uses.
  When a recheck finds the two agreeing, a previously published
  `unconfigured` state is cleared. Shown regardless of the timing toggles:
  a chosen provider with no setup is worth one line on the menu.
- New text `Textes\UI\CloudSync\unconfigured`: "Cloud: storage not set up -
  Settings > Cloud > Config...". No placeholder substitution; the indicator
  texts are static strings and stay that way. `CloudFailureTextKey` in
  `MainMenu.cpp` learns the outcome.

## Verification

- Zig: unit tests for `bk_cloudsync_creds_backend` with a document present
  and absent; `tools/zig/cloudsync_facade_test.cpp` exercises
  `CredentialsBackend`. Existing suites stay green (creds 19, catalogue 20,
  form 9, worker 17, engine 20).
- Game: GameTT screens have no unit tests; the evidence is the headless
  `BK_AUTO_UI` route in the release build against the three loopback
  services of P04-M01. Required captures: the Provider row stepped by row
  click and by arrow key; Config… opened, filled, tested, saved; `cloud
  sync: startup sync begun` in the trace on the next launch; the
  `unconfigured` indicator on a profile with a provider and no credentials;
  a `shot` of the tab with the buttons inside the box, and one with
  Provider `Off` showing a single row and no buttons; and OK pressed with
  no clicks leaving `Cloud.Provider` unchanged — the entry-0 trap.
- The P04-M01 evidence's harness recipe walks the dialog's chooser, which
  no longer exists; the evidence file gets a note that the recipe is
  historical and the row is the new route.

## Out of scope

Per-identity pairing records and the confirm-repair flow (discussed
2026-08-30, deferred together); any change to the credentials document
schema; provider display names.
