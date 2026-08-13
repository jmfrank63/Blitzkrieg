# Player profiles: full profile management

Date: 2026-08-13. Requested by Johannes in-session. Builds on the profile
storage layer of 2026-08-11 (per-profile `config.cfg`, saves, screenshots)
and the 2026-08-13 mojibake fixes.

## Intent

A profile is a complete, switchable set of per-player data: settings,
saves, autosaves, replays and screenshots. Switching profiles swaps the
whole set. The player-profile screen grows from "type a name, OK" into a
small manager: it lists the existing profiles and can create, rename and
delete them. The profile name shown always defaults to `Player` and can
never be garbage.

## What already exists (verified 2026-08-13)

All per-player storage is already profile-scoped through
`NProfile::Segment()` (`Sources/src/StreamIO/ProfilePaths.h`):

- **Settings** — `profiles/<name>/config.cfg` (root `config.cfg` is a
  read-only fallback for migration).
- **Saves and autosaves** — `profiles/<name>/saves/` and
  `profiles/<name>/mods/<mod>/saves/` (`Sources/src/Main/MainLoopCommands.cpp`;
  save/load dialogs list through the same segment).
- **Replays** — same tree (`SaveReplay.cpp`, `ReplayList.cpp`).
- **Screenshots** — `profiles/<name>/screenshots/`
  (`Sources/src/Main/iMainInternal.cpp`, `CMD_SCREENSHOT`).
- **Bootstrap** — `-profile=Name` beats `profiles/active.cfg` beats
  `"Player"` (`GameMain.cpp`); first run migrates legacy root `saves/` and
  `screenshots/` into the first profile.
- **Switch/create** — the PlayerProfile screen
  (`Sources/src/GameTT/InterfaceStartDialog.cpp`, layout
  `Data/UI/PlayerProfile.xml` + `.lua`): OK with a new name flushes the old
  profile's config, creates/loads the new profile's directories and config,
  and rewrites `active.cfg`. The player name IS the profile name.
- **Hygiene (2026-08-13)** — `NProfile::Sanitize` keeps only printable
  ASCII (mojibake from the wide→narrow truncation made invalid-UTF-8 paths
  that APFS rejected, silently killing every save); a poisoned `active.cfg`
  self-heals to `Player` at startup; the dialog's edit box shows the active
  profile name (always sanitized, `Player` by default), not the raw
  `GamePlay.PlayerName` option.

What is missing is purely management: listing, explicit create, rename,
delete, and the UI for them.

## Definitions

- **Profile name** — the directory name under `profiles/`. Printable ASCII
  only (32..126 minus `/\:*?"<>|`), no leading/trailing spaces, no trailing
  dots, non-empty after sanitizing (else `Player`). The typed player name
  is sanitized into this; `GamePlay.PlayerName` keeps whatever was typed
  for display and multiplayer.
- **Active profile** — `Profile.Name` global, persisted in
  `profiles/active.cfg`.
- **Profile list** — the filesystem is the registry: every directory
  directly under `profiles/` whose name survives `Sanitize` unchanged is a
  profile. No index file to drift out of sync.

## Config layering (verified 2026-08-13)

Settings resolve through two files with distinct roles
(`SerializeConfig` / `ResolveConfigFileName`,
`Sources/src/Main/iMainInternal.cpp`):

- **`config.cfg`** — the player's saved settings. Read resolution order:
  `profiles/<name>/config.cfg` → root `config.cfg` → `Data/Configs/config.cfg`
  (plus parent-directory fallbacks for tools). The profile copy, when it
  exists, beats everything below it. **Writes always land in the active
  profile's copy** and never touch the root file — the root `config.cfg`
  survives untouched as the profile-less fallback and as the seed a brand-new
  profile first reads (migration by fallback: first read falls through to the
  root, first flush materializes the profile's own copy).
- **`defconf.cfg`** — shipped default values, resolved from the root /
  `Data/Configs` only (never per-profile). On every config read it is loaded
  as the *repair tree*: options missing from the player's config are filled
  from it. It is never written.
- **No profile at all** (`Profile.Name` unset — editor/tool contexts that
  skip GameMain's bootstrap): reads and writes use the root `config.cfg`
  directly, so the game and tools work profile-less, exactly as before
  profiles existed.
- **`-profile=<Name>`** — already supported: GameMain parses it, it beats
  `profiles/active.cfg` (which beats the `"Player"` default), the profile is
  created on demand, and the choice persists as the new active profile
  (same persistence convention as `-fullscreen`/`-monitorN`). A regression
  where `main.cpp`'s argument whitelist rejected the flag was fixed
  2026-08-13. Verified end-to-end: `-profile=Cmdtest` created
  `profiles/Cmdtest/{config.cfg,saves,screenshots}`, made it active, and
  flushed settings into it.

## Behavior

### Screen layout

The PlayerProfile screen keeps its edit box and OK/Cancel and gains:

- a **list control** of existing profiles (alphabetical, active profile
  pre-selected). Selecting an entry puts its name in the edit box.
- **New** — clears the edit box (focused) so OK creates whatever name is
  typed; until OK nothing exists.
- **Rename** — renames the profile selected in the list to the edit box
  content.
- **Delete** — deletes the profile selected in the list after an "are you
  sure" confirmation dialog (the standard yes/no dialog).

OK keeps today's semantics: sanitize the edit box; same name → just save
the player-name option; different name → switch to (creating if needed)
that profile. Cancel leaves everything untouched.

### Rules

- **Create**: directory trio (`saves`, `screenshots`, plus `config.cfg` on
  first flush) seeded from the settings in effect right now (today's
  behavior). Name collisions are not an error — OK on an existing name
  switches to it.
- **Rename**: filesystem rename of `profiles/<old>/` to `profiles/<new>/`.
  Renaming the active profile flushes its config first, then renames, then
  updates `Profile.Name`, `active.cfg` and `GamePlay.PlayerName`. Renaming
  onto an existing profile name is refused (message, no action). A
  case-only rename must go through a temporary name (APFS and NTFS are
  case-insensitive).
- **Delete**: recursive removal after confirmation. Deleting a non-active
  profile changes nothing else. Deleting the active profile switches to
  the first remaining profile (alphabetical); if none remain, a fresh
  `Player` profile is created seeded with current settings. The list can
  therefore never be empty after the screen closes.
- **Failure reporting**: any filesystem failure (rename/delete returning
  an error) writes a red line to the chat console — the save path taught
  us silent failure is unacceptable.

### Entry points (unchanged)

- First run: the screen auto-opens when the stored player name is empty or
  equals the localized default (`CUINewGameState::Show`).
- Menu: the Player Profile button (`IMC_PLAYER_PROFILE`) in the New Game
  screen.

## Edge cases

- `-profile=Name` names a profile deleted mid-session earlier: startup
  recreates it (already the effect of `create_directories`).
- Deleting or renaming while a mission is running: the screen is only
  reachable from the main menu flow, so no live mission holds profile
  paths open; save/load always rebuilds paths from `Profile.Name`.
- A `profiles/` directory the user created by hand with an invalid name is
  not listed (fails the `Sanitize`-unchanged test) and never touched.
- Legacy wide player names (pre-ASCII builds) still display via
  `GamePlay.PlayerName` in multiplayer but never reach the filesystem.

## Non-goals

- No per-profile keybindings split (they live in the config already and
  follow it).
- No profile import/export or copy.
- No Steam-cloud style sync.
- No change to the "player name is the profile" identity model.
