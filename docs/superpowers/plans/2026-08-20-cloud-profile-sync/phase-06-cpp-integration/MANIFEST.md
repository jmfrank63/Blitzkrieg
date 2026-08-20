# Phase 06 — C++ Integration and Lifecycle

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Connect the engine to the game: a facade, the three sync points, and a visible state.

| Packet | Depends on | Owns |
|---|---|---|
| P06-M01 | P02-M05, P03-M04, P04-M04, P05-M02 | C++ facade over the C ABI, including the pending-restore apply and discovery-status wrappers |
| P06-M02 | M01 | startup pull before the profile config is read |
| P06-M03 | M02 | post-save and exit push |
| P06-M04 | M03 | sync indicator and skip-to-offline |

Exit: the game starts, syncs, and exits on every target with no socket call on the main thread.

P06-M01 Windows checkpoint: `zig build test-cloudsync-facade -Dtest-mode=run`
passes both run modes (library absent → every call degrades; library present
→ full surface callable, credential-less Begin failing cleanly);
`install-game` stages `CloudSync.dll`; Linux cross-targets compile. Commit
`c8819e9b5`.

Carried forward from P06-M01:

- **The facade loads the library at runtime** (LoadLibrary/dlopen), which is
  what makes "a build without CloudSync still links" true. Only
  `ApplyPendingRestore` bypasses the availability notion — a staged restore
  must finish with the feature off; a missing *library* makes it a quiet
  no-op returning 0.
- `Begin(profile)` hides the pair-vs-sync decision: a sync whose failure is
  the exact `NotPaired` text retries once as a pairing on the same facade
  handle. Callers never see the fallback.
- The fingerprint is derived facade-side from the redacted credentials
  document (endpoint/bucket/url, no secrets); only self-consistency
  matters, and Zig's `creds.fingerprint` is not consulted.
- The facade is C-runtime-only (no STL) so its test binary stays out of the
  MSVC RuntimeLibrary fight; keep it that way when P06-M02 compiles it into
  the game.
- `-fentry=main` binaries never get argv — the CRT is bypassed. Modes and
  parameters for such test exes travel by environment variable.
- `CloudSyncFacade.cpp` is compiled only by its test so far. P06-M02 owns
  adding it to the game build (Main.vcxproj or the exe's source list)
  together with the first call site.

P06-M02 Windows checkpoint: measured end to end on the release build against
a live MinIO — launch one paired before the menu (begun pre-menu-pipeline,
finished during menu load, frame cadence unchanged), launch two synced
steady-state, the bucket holding the profile's actual saves with the
sentinel and `config.cfg` absent, `cloudsync/state/USSR.json` carrying the
fingerprint. All suites green. Commit `caaf3ccbe`.

Carried forward from P06-M02:

- **Two real defects found by the measurement.** (1) The minimal config
  scan's first draft read the wrong element: inside an item the `<Default>`
  block's own `<Var>` sits between the live value and `<KeyName>`, so
  nearest-preceding-`<Var>` is always the default — the scan anchors at the
  enclosing `<item` and takes the first `<Var>` after it. (2) Discovery
  honoured the credentials `rclone_path` only after a live `creds_save`; the
  library now bootstraps the explicit override from `cloud.credentials` once
  per process before the first probe (`cloudsync.zig` amended beyond the
  allowlist, recorded).
- The platform audit rejects raw `LoadLibrary`/`dlopen` outside approved
  paths; the facade's native corners live in
  `Sources/src/Platform/CloudSyncLoader.*` (path-scoped adapter), and the
  facade itself has no platform includes.
- `CloudSyncFacade.cpp` + `CloudSyncLoader.cpp` are compiled into the game
  via `game_sources`; the facade test compiles all three files itself.
- Harness lessons: `set=` writes the option system directly, but an open
  settings screen's OK/cancel overwrites it with the screen's captured
  values — set options with no screen open. The game's own traces
  (`NStr::DebugTrace`) do reach stderr in headless release runs.
- The main-loop poll block traces and releases on settle; P06-M04 replaces
  the traces with the indicator. P06-M03 owns exit (`Shutdown()` is not yet
  called anywhere in the game; the Windows job object reaps the daemon on
  process death, POSIX relies on next-launch identity reaping).

P06-M03 Windows checkpoint: measured on the release build against a live
MinIO — three `cmd=0x10010014` saves in one second produced exactly one
"post-save sync begun" and one "(synced)", the exit path traced "finishing
before exit" then "exit sync finished", and `Get-Process rclone` was empty
after the game exited. All suites green offline and live; facade
cross-compiles pass for `x86_64-linux-gnu`, `aarch64-linux-gnu`,
`x86_64-windows-gnu`. Commit `578717fa3`.

Carried forward from P06-M03:

- **The save signal is a global-var counter, not a callback.** Every save —
  quick, auto, mission dialog — funnels through
  `CMainLoop::Command(MAIN_COMMAND_SAVE, ...)` in `iMainInternal.cpp`, which
  bumps `CloudSync.SavesSeen`; `GameMain.cpp`'s main loop diffs the counter
  and re-arms a five-second quiet window on every bump, so a save burst
  coalesces into one sync. The counter bumps when the command is *issued*,
  before the file write lands — the quiet window is also what keeps the
  push behind the actual write.
- The post-save push holds while `AreWeInMission` is set (saves during play
  re-arm the window; the push fires after mission end), while a handle is
  already active, and unless `Cloud.Enabled` + `Cloud.Sync.OnSave` read ON
  via the live option system (`CloudSyncOptionOn`, not the config-file scan
  — by main-loop time the option system is up).
- The exit push sits *after* `SerializeConfig(false, ...)` so a config
  backup snapshots the final settings; it fires on `Cloud.Sync.OnExit` OR a
  still-pending coalescing window (a pending post-save push is flushed, not
  dropped). Bounded wait: 15 s polling at 100 ms with traces as the
  indicator; on timeout the run is abandoned — the profile simply stays
  ahead of the cloud and the next startup pull converges.
- `NCloudSync::Shutdown()` now runs unconditionally on the exit path
  (inside the same block, after the optional wait). A crash skips it; the
  Windows job object and next-launch identity reaping still cover the
  daemon.
- `MAIN_COMMAND_SAVE` is `0x10010014` (`MAIN_BASE_VALUE + 20`) — usable as
  a `cmd=` harness injection to trigger the counter from the menu; the
  save itself fails harmlessly with an empty filename, which is exactly
  what the coalescing test wants.
- `zig build game -Dtarget=<linux/windows-gnu>` does NOT cross-configure
  from Windows (SDL3 `.so` symlink needs Developer Mode/elevation; the
  full game's cross libc setup is a separate open item). The game-side
  cross gate remains `test-cloudsync-facade -Dtest-mode=compile`, which
  compiles the facade + loader C++ for the three cross targets.

P06-M04 Windows checkpoint: the indicator narrates every state on the
release build at 1024x768 (evidence/cloud-sync/indicator/, fourteen
screenshots): pairing/syncing amber with the skip affordance, the
click-to-offline flow measured live against a hung remote ("skip to
offline requested" traced, cancel settling as "Cancelled"), paired/synced,
and a distinct message per P02-M03 outcome — auth_failed (cleared secret),
too_many_deletes (saves moved out), needs_resync (listings deleted) staged
for real, the rest via the harness vars the indicator renders from. All
suites green offline and live; facade cross-compiles pass. Phase 06 closed.
Commit `249bd2339`.

Carried forward from P06-M04 (all of it earned by measurement):

- **The menu screen is modal.** The exit-confirm dialog's `ModalFlag="1"`
  makes `InitDependentInfoMW` set `bModal` on the whole screen, and the
  modal branch of `CMultipleWindow::OnLButtonDown` routes every press to
  the FRONT child only (the active state container, moved up by its
  `Show()`), swallowing the rest of the screen. Screen-level siblings are
  unreachable by mouse picking, and the notify-to-game translation only
  exists in per-container Lua anyway. The indicator therefore stays a
  plain element to the picker; `CInterfaceMainMenu::ProcessMessage`
  hit-tests the unconsumed `CMD_END_ACTION1` (packed position, cursor
  fallback) against the element's own rect. The button-DOWN is eaten by
  the screen; the UP is what falls through.
- `AddChild` is `push_front`: the LAST XML child is picked first and drawn
  last. The indicator sits at the end of `MainMenu.xml`'s children.
- The main loop publishes `CloudSync.State/Outcome/Error` while a handle
  is live and consumes `CloudSync.SkipToOffline` into `Cancel` (also
  swallowing a click that raced the settle so it cannot cancel a future
  run). The menu never touches the facade.
- The worker now leads pair/sync `SyncFailed` texts with the classified
  outcome tag (`publishRunFailure`, the testConnection contract) and
  publishes bare "Cancelled" for a cancellation — the engine's stored
  text can belong to an earlier run, and the facade's exact-match
  `NotPaired` retry is deliberately untouched.
- The skip choice is sticky in the menu (`bCloudSkipRequested`): a hung
  transfer's eventual failure text must not overwrite the player's
  answer; a settled-to-running edge (new run) clears it. Cancel during a
  hung synchronous rc call settles only when that call returns — the exit
  path's bounded 15 s wait plus `Shutdown()` covers a player who quits
  immediately after skipping.
- A bucket that vanishes on an already-paired profile classifies
  `needs_resync` on the SYNC path (bisync aborts on its own listings
  before the remote answers) — and resync genuinely is the repair there.
  `remote_missing` belongs to the `testConnection` probe (P07's settings
  dialog). A wrong-but-present secret can also surface as `unknown:` noise
  from bisync; the cleared-secret probe text is the reliable auth staging.
- `-mode=1024x768` pins the headless resolution (the mode option path);
  the default otherwise chases the desktop. Element 21001's rect at that
  resolution is x 10..650, y 614..664 — `click=150x640` hits it.
- Disabled buttons render sub-state-3 grey (`0xffa0a0a0` default): the
  settled indicator goes grey for free via `EnableWindow(false)`.
