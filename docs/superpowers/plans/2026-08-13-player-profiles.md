# Player Profiles Management Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement docs/superpowers/specs/2026-08-13-player-profiles-design.md — the PlayerProfile screen lists existing profiles and can create, rename and delete them; all per-player data (settings, saves, autosaves, replays, screenshots) already lives under `profiles/<name>/` and switching swaps the whole set.

**Architecture:** The filesystem stays the registry (directories under `profiles/`), `Profile.Name` + `profiles/active.cfg` stay the active pointer, and the existing OK-switch logic in `CInterfacePlayerProfile` stays the only way a profile becomes active. New code is: a small profile-ops helper (list/rename/delete with error reporting) beside `NProfile` in StreamIO, list + three buttons in the PlayerProfile UI layout, and message handling in `CInterfacePlayerProfile`. No new persistence format anywhere.

**Tech Stack:** C++ (legacy engine), data-driven UI (`Data/UI/*.xml` + `.lua`), Zig build. Build: `zig build install-game -Dtarget=aarch64-macos --release=fast` from the repo root. Runtime verification headless: `BK_AUTO_UI` (see the memory notes / docs) from `zig-out/game/macos/arm64/release`; Windows build verified on the Windows machine.

## Global Constraints

- Profile names: printable ASCII per `NProfile::Sanitize`; the sanitized name IS the directory name. Never write a path from an unsanitized string.
- `active.cfg` may only ever contain a sanitized name (it self-heals at startup, but do not rely on that for new writes).
- Every filesystem failure surfaces in the chat console (red), like the save-failure message in `CICSave::Exec` — no silent failure.
- After the screen closes there is always at least one profile and it is active.
- Case-insensitive filesystems (APFS, NTFS): equality checks on profile names are case-insensitive; case-only rename goes through a temp name.
- The legacy D3D9 path shares the GameTT code — no GPU-only APIs in this feature.
- MSVC and clang must both build it (use `std::filesystem` with `std::error_code`, no exceptions).

---

### Task 1: Profile operations helper

**Files:**
- Modify: `Sources/src/StreamIO/ProfilePaths.h` (namespace `NProfile`)

**Interfaces:**
- `NProfile::List() -> std::vector<std::string>` — sorted names of directories directly under `profiles/` whose name survives `Sanitize` unchanged (case-insensitive compare for sorting).
- `NProfile::Rename( const std::string &from, const std::string &to, std::string *pError ) -> bool` — directory rename, two-step through `profiles/<to>.tmp-rename` when only case differs; refuses when `to` exists (case-insensitive) and is not `from`.
- `NProfile::Delete( const std::string &name, std::string *pError ) -> bool` — `std::filesystem::remove_all` with `error_code`.
- All header-only (callers span dylibs, same reason `Segment`/`Sanitize` are inline).

- [x] **Step 1:** Implement the three helpers. `List` uses `std::filesystem::directory_iterator("profiles", ec)`; skip non-directories and names where `Sanitize(name) != name`.
- [x] **Step 2:** Build both targets locally (`zig build install-game ...`). Build clean; the no-behavior-change runtime check is folded into the Wave-2 verification run.

### Task 2: UI layout — list and buttons

**Files:**
- Modify: `Data/UI/PlayerProfile.xml` (add a profile list control and New / Rename / Delete buttons; follow the existing E_LIST options-list markup in the same file for the list classes and the OK/Cancel items for button markup)
- Modify: `Data/UI/PlayerProfile.lua` (forward the three new button message IDs like OK/Cancel are forwarded)
- Modify: `Data/Textes` (new keys under `Textes\UI\Intermission\MainMenu\PlayerProfile\`: `new`, `rename`, `delete`, `confirm_delete`, `err_exists`, `err_fs`; mirror however the existing `caption` key is stored per language)

**Interfaces:**
- New ElementIDs in the XML consumed by Task 3 (pick free IDs near the existing E_EDITBOX/E_BUTTON_OK ones in `InterfaceStartDialog.cpp`'s enum).

- [x] **Step 1:** Add the list + buttons to the XML with placeholder positions; wire the Lua forwards. (List 2100 modeled on LoadMission's; buttons 10010/10011/10012 with TextKeys; dialog enlarged to 736x410; selection notify 536936451 and double-click-as-OK forwarded.)
- [x] **Step 2:** Verify layout headless and iterate. Done, including a styling pass to Johannes's requirement (gold text + standard frames): dialog grown to 736x520, right column in its own 9-piece-frame panel with opaque backing, gold row template `Data/UI/common/goldlefttext.xml` (new), MainMenu-style gold button labels, V/X re-paired under the left plate. CUIList needs a per-column header child (`GetChildByID(10+i)`) or Reposition null-derefs — now documented in the list markup.

### Task 3: Screen logic — list, select, create

**Files:**
- Modify: `Sources/src/GameTT/InterfaceStartDialog.cpp` / its header (class `CInterfacePlayerProfile`)

**Interfaces:**
- Fills the list from `NProfile::List()` in `StartInterface`, pre-selects the active profile.
- List selection → edit box text.
- New button → clears edit box, focuses it (create happens through existing OK path).

- [x] **Step 1:** Populate the list and hook selection. OK/Cancel behavior unchanged. (FillProfileList in StartInterface; selection→edit box via forwarded notify 2100; New clears+focuses the edit box. A CUIList quirk surfaced: each column needs a header child `GetChildByID(10+i)` or Reposition crashes — a zero-height header static was added to the list markup.)
- [x] **Step 2:** Headless test: both profiles listed, active pre-selected, row click fills the edit box — verified with screenshots and disk checks.

### Task 4: Rename and delete

**Files:**
- Modify: `Sources/src/GameTT/InterfaceStartDialog.cpp`

**Interfaces:**
- Rename button: `NProfile::Rename(selected, Sanitize(editbox))`; active-profile rename flushes config first (`SerializeConfig(false, ...)` like the OK path), then updates `Profile.Name`, `active.cfg`, `GamePlay.PlayerName`. Errors → chat console + `err_*` text in the dialog.
- Delete button: raise the standard yes/no confirmation dialog (reuse the pattern other GameTT screens use, e.g. the delete-save confirmation in the load screen); on yes `NProfile::Delete`; if the active profile died, switch to the first remaining or recreate `Player` (reuse the OK-switch code path — factor it into a private `SwitchToProfile(name)` so OK, delete-fallback and rename share it).

- [x] **Step 1:** Factor today's OK-switch body into `SwitchToProfile`; OK calls it (no behavior change — switch verified headless: OK on a selected profile updates active.cfg and the profile's config).
- [x] **Step 2:** Implement rename with the collision and case-only rules. Runtime-verified: same-name no-op, empty-selection/empty-edit guards, error surfacing (localized text + raw fs message, red, in chat console). Collision refusal and case-only temp-hop verified by a standalone compiled test of NProfile::Rename; the button path shares that code but typing a new name headless isn't possible — re-check interactively at sign-off.
- [x] **Step 3:** Implement delete + confirmation (standard MISSION_COMMAND_MESSAGE_BOX; answer consumed in OnGetFocus via a pending-name member). Runtime-verified: confirm/cancel, delete inactive, delete active (falls back to first remaining, active.cfg consistent). Delete-of-last-profile (fresh `Player` recreation) is inspection-only — needs an empty profiles/ dir; exercise at sign-off.

### Task 5: Cross-platform pass and sign-off

- [ ] **Step 1:** Windows build + smoke on the Windows machine (paths, `std::filesystem` rename/delete semantics on NTFS, MSVC build).
- [ ] **Step 2:** Sign-off checklist with Johannes: fresh install shows `Player`; garbage `active.cfg` self-heals; create/switch/rename/delete round-trip keeps saves, autosaves, screenshots and settings with their profile; each profile's `GFX.Mode`/fullscreen setting applies on switch.
- [ ] **Step 3:** Update `docs/scaling.md`-style docs: extend the profile section in `docs/superpowers/specs/2026-08-13-player-profiles-design.md` with anything learned, and refresh the memory notes.
