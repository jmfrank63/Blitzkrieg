# Phase 3: Map editor plan 6: finish M1 - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-09-28
**Phase:** 03-map-editor-plan-6-finish-m1
**Areas discussed:** Test-launch in the game, Camera rotate and zoom, Where maps live and saving, Editor settings and mods

---

## Test-launch in the game

**Unsaved changes on Test** — options: Test a temporary copy / Save first, then launch / Ask each time

**User's choice:** Test a temporary copy; the map file is untouched until you save; no prompt

**Profile** — options: Separate editor test profile / Current game profile

**User's choice:** A separate editor test profile (e.g. MapEditorTest)

**Editor while the game runs** — options: Stays open / Editor waits, greyed out / You decide

**User's choice:** Stays open; switch back; quitting the game returns to the editor

**Side** — options: Player 0 / Choose in a dialog / You decide

**User's choice:** Player 0, as the map defines

**Window** — options: Windowed / Like the game's own setting

**User's choice:** Windowed, beside the editor, ignoring the test profile's fullscreen

**Test already running** — options: Offer to restart / Always restart / Refuse

**User's choice:** Offer to restart it (close running test and start new, or keep)

**Start point** — options: Normal mission start / With briefing

**User's choice:** Normal mission start, briefing skipped

**Game build** — options: Game beside the editor / Configurable path

**User's choice:** The Game executable beside MapEditor

---

## Camera rotate and zoom

**Zoom input** — options: Like the game / Plain wheel zooms / Keys only

**User's choice:** Like the game: trackpad pinch and Shift+wheel/swipe; plain swipe keeps panning

**Range** — options: Game's limits / Freer than the game

**User's choice:** The game's zoom limits and view angles

**Rotate input** — options: Trackpad rotate + Alt+Q/E / Right-drag / 90-degree snaps

**User's choice:** Trackpad rotate gesture and Alt+Q/E turn the camera; Q/E stay for the selected object

**Reset** — options: Yes / No

**User's choice:** Yes: a key (e.g. Home) and a menu item reset rotation and zoom to the game's default view

**Zoom centre** — options: Pointer / Screen centre

**User's choice:** On the pointer

**Persist view** — options: Per map, session / Saved across sessions / Always centred

**User's choice:** Per map, for this session only

**Whole-map overview** — options: No / Overview key

**User's choice:** No; game's limits are enough in M1 (minimap tools are M3)

---

## Where maps live, saving

**Default maps folder** — options: User-data maps folder / Ask each time / Data/Maps

**User's choice:** A 'maps' folder in the user data area beside profiles/saves, where the game's custom-mission list can find them; shipped Data maps never overwritten

**Save on a shipped map** — options: Save As / Overwrite with warning

**User's choice:** Save becomes Save As (shipped maps read-only), defaulting to the user maps folder

**Backup** — options: One .bak / No backup / Numbered history

**User's choice:** Safe save (temp file + swap) plus one name.bzm.bak; the .bak is taken once per session at the first write, holding the version from when the map was opened

**Autosave** — options: Recovery copy only / No autosave / Autosave into the map

**User's choice:** Autosave writes into the map itself

**Autosave interval** — options: Every 2 minutes / Every 5 minutes / Configurable

**User's choice:** Configurable in settings (default 2 minutes, only when there are unsaved changes)

**Never-saved map** — options: Recovery copy until first save / No autosave until first save

**User's choice:** Recovery copy in user data until the first Save As; afterwards autosave writes into the file

**Autosave toggle** — options: Yes, on by default / Always on

**User's choice:** Switchable (setting and menu toggle), on by default

**Unsaved-changes prompt** — options: Save/Don't save/Cancel / Save/Cancel

**User's choice:** Save / Don't save / Cancel on Open, Quit and window close; Save on new or shipped maps goes through Save As

---

## Editor settings and mods

**Settings location** — options: Own file in user data / Inside the active game profile

**User's choice:** Its own file in the user data (e.g. mapeditor.cfg), independent of game profiles

**Settings UI** — options: Settings window / Only the settings file

**User's choice:** A Settings window in the editor

**Choosing a mod** — options: Menu and command line / Command line only

**User's choice:** File -> Mod menu and -mod=Name on the command line; switching reloads the palette

**Recent files** — options: 10 / 5 / Configurable

**User's choice:** 10, in File -> Open Recent; missing files greyed and removable

**Mod maps folder** — options: Mod's own maps folder / Same maps folder

**User's choice:** The mod's own maps folder (mods/<Name>/maps in user data)

**Test game mod** — options: Editor's mod / Test profile's own mod

**User's choice:** Always the editor's mod (-mod=Name / -mod=None)

---

## Claude's Discretion

Object icons, brush outline, panels following a resize, sound list editor, unknown-objects warning, BK_EDITOR_AUTO and shot comparison, the full sweep, packaging (incl. Windows console subsystem), plan 5 deferred minors; recovery-copy location, settings file format, view-reset key.

## Deferred Ideas

- Whole-map overview / zoom beyond the game limits — with M3 minimap tools
