# Phase 5: Discussion Log (smart discuss, autonomous)

**Date:** 2026-09-30
**Mode:** Autonomous smart discuss. The user's standing order is "always choose the recommended answer, never come back with questions", so every grey area was resolved with the recommended answer. The rejected alternatives are recorded here.

**Inputs:**
- `03-CONTEXT.md`, the portable map editor spec, `PROJECT.md`, `REQUIREMENTS.md`, `999.1-NOTES.md`;
- a codebase scout of `Sources/editor`, `Sources/src/EditorBridge` and `build.zig`;
- three read-only inventories of `Sources/src/MapEditor`: menus and frame, tabs and tool states, RMG and minimap. They are condensed into `05-PARITY.md`.

**Inventory findings that shaped the answers:**
- In the MFC editor, undo, cut, copy and paste are dead. So are the storage-coverage overlay, the Sounds, Forests and RMG tabs, and the Templates "Check!".
- `ResizeDialog` is not a map-resize feature. The MFC editor has no map resize.
- The MFC save silently runs `CheckMap(false)` and recomputes shades over the whole map. Minimap images are written only by an explicit button.
- The composers' `Editor\Default*.xml` list files are not shipped, so the MFC composers open empty.
- `CreateRandomMap` stores the chapter name in `szMODName` (`MapInfo_StaticMethods_RMGeneration.cpp:971`).
- `RandomMapGen` is already linked into the portable editor, and the random-missions tier already drives `CreateRandomMap` headless.

---

## Grey area 1/4: Random map generation and templates

| # | Question | Chosen (recommended) | Rejected alternatives |
|---|---|---|---|
| 1 | Where do generated maps go, and what happens after? | User or mod maps folder, then open as a normal document (D-02) | Write into `Data\maps` like the MFC editor: breaks shipped-data read-only (spec D-18). Generate only to a temp file for Test in game: loses the MFC ability to keep generated maps |
| 2 | How does the long generation (2.5–15 s in debug) run? | Main thread, progress modal pumped by `IProgressHook`, no cancel (D-03) | Worker thread with cancel: `CreateRandomMap` uses global engine state (object DB, storages, `rand`), so it is unsafe. Blocking without progress: the window freezes and the OS flags it as hung |
| 3 | Pull 999.1's fill speed-up into M3? | No. Add the determinism harness only (D-04) | Pull it in: 999.1 is a performance item with its own gate, not a parity feature, and it would widen M3's risk |
| 4 | Seed control? | Optional seed field; the used seed is shown and stored (D-01) | No seed (MFC parity only): no reproducible generation, and the byte-identical tests could not be written |
| 5 | Where do user-authored RMG files live? | `<UserRoot>rmg/` (mod: `<UserRoot>mods/<Folder>/rmg/`), mirroring `Data`, mounted as a storage layer (D-09) | Write into `Data` (the MFC behaviour): violates read-only shipped data. A flat folder not mounted as storage: references between RMG files resolve through storage, so the files would not load in `CreateRandomMap` |
| 6 | How are composer lists populated? | Scan the storage folders (D-08) | Keep the `Editor\Default*.xml` list files: they are not shipped, so the lists would start empty as in the MFC editor |
| 7 | Patch outside the storages? | Offer to copy it into the RMG root (D-10) | Refuse, like the MFC editor: a user map in `<UserRoot>maps` could never become a patch |
| 8 | Composer UI form | One dockable ImGui window per composer, with tables, context menus and a draw-list graph canvas (D-06, D-11) | One combined "RMG workbench" window: diverges from the MFC structure and makes parity harder to show row by row. Modal dialogs: blocks map viewing while composing |
| 9 | Check! behaviour | Report first; removal is explicit and undoable. The Templates Check! is implemented (D-12) | Silent removal (MFC): surprising data loss. Leave Templates Check! dead: it is cheap to make useful |
| 10 | Tools 0–3 output | `<UserRoot>mapeditor/logs/` (D-13) | The data folder `logs\` (MFC): writes beside shipped data |

## Grey area 2/4: Minimap

| # | Question | Chosen (recommended) | Rejected alternatives |
|---|---|---|---|
| 1 | How is the minimap drawn? | CPU raster into a texture, the MFC Editor mode, refreshed incrementally (D-14) | Render a second engine camera off-screen: GFXGPU has no multi-view path, and it is heavier. Show only the pre-built image: stale while editing |
| 2 | Interaction | Click/drag moves the camera (D-15) | Draggable viewport rectangle: commented out in the MFC editor, so not parity. Could be a later nicety |
| 3 | When are minimap images written? | Explicit Map → Create Minimap Images (D-17) | On every save: changes save output beyond the edits (preservation invariant), and the MFC editor did not do it. Setting-controlled auto-create: adds a mode for no parity gain |
| 4 | Game mode | Shown when the image exists (D-16) | Drop it: the MFC editor has it |

## Grey area 3/4: Terrain, objects and app parity

| # | Question | Chosen (recommended) | Rejected alternatives |
|---|---|---|---|
| 1 | Heights edits vs the preservation invariant | Extend the invariant: altitudes are editable through a deterministic region function with region shades (D-19) | Whole-map shade recompute at save (MFC): every save changes all shades and breaks "untouched is byte-identical". Heights without shade update: the view and the saved map diverge |
| 2 | Update Map | An explicit undoable command, plus the Instant Update toggle (D-20) | Run it implicitly at save: same preservation problem |
| 3 | Middle-button actions on a trackpad | Alt+drag / Alt+click equals middle (D-18, D-29) | Middle button only: unusable on Johannes's MacBook |
| 4 | Check Map and save | Explicit check, report panel, undoable Fix all; save only warns (D-33) | Silent fix on save (MFC): hidden data changes |
| 5 | Unknown objects on a mod change | Keep M1's preservation; Check Map offers explicit removal (D-33, F4) | Auto `RemoveNonExistingObjects` (MFC): M1 already rejected it in the spec |
| 6 | Save as XML/BZM shortcuts | Ctrl+Shift+X / Ctrl+Shift+B (D-24) | Ctrl+X / Ctrl+B (MFC): Ctrl+X clashes with Cut in ImGui text fields |
| 7 | Brush size | 1–16, even sizes included (D-22) | Keep M1's radius 0–4: short of MFC's 16×16 |
| 8 | Links (garrison, tow, couple) | Drop-to-link with the MFC rules. Deleting a host deletes its passengers in one undo step (D-27) | Keep M1's refusal: a parity gap, since the MFC editor can do it |
| 9 | Help | An in-app keys and tools window, plus About (D-34) | Ship or port the `.chm`: not shipped, Windows-only |
| 10 | Single instance | A per-user local IPC hand-off (D-34) | Skip it: the MFC editor can do it, and the user wants everything |
| 11 | Toolbars and Customize | View menu panel toggles, ImGui docking, Reset layout (D-34) | Rebuild MFC-style toolbars with customisation: ImGui docking already gives an equivalent rearrangement |
| 12 | MFC bugs | Implement the evident intent and note it in the parity row | Replicate the bugs: fill rect, tile 0, damage null pointer, layer desync, `szMODName` |

## Grey area 4/4: Parity proof, testing and deletion

| # | Question | Chosen (recommended) | Rejected alternatives |
|---|---|---|---|
| 1 | Parity proof form | `05-PARITY.md`: one row per feature, owner, plan and evidence; NF rows need file:line (D-36) | A prose summary: not checkable. Only automated coverage: some rows need a hand-try note |
| 2 | Test standard | Core + map-file overlay + engine tests, and a `map-editor-m3-auto` scenario on macOS and win-home (D-37) | App tier only: misses preservation regressions |
| 3 | Deletion gate | Last plan, after every row including phase 4's is closed (D-38) | Delete as soon as M3's own rows close: M2's rows would lose their reference |
| 4 | What else goes with the deletion | The checked-in `MapEditor.exe`, the sln entry, the VS Code task, the stage/install entries, the openspy assert. `RandomMapGen` and `Data/Editor` stay (D-38) | Delete `RandomMapGen` too: the game uses it. Keep `bin/MapEditor.exe` for packaging: packaging the old 32-bit editor after parity has no purpose |
| 5 | Plan split | 11 plans in the waves of D-39 | Four large plans (terrain / objects / RMG / cleanup): too big to verify. One plan per parity row: far too fine |

## Claude's discretion

- ImGui layouts and names.
- Keys the MFC editor lacks.
- The minimap refresh rate.
- The IPC mechanism.
- The internal form of the altitude region record.
- A shared list/properties widget.

## Deferred

- The game's Custom Mission list for user maps and templates (a game feature).
- The 999.1 speed-up.
- Camera rotation (phase 4).

Nothing the MFC editor can do was deferred.
