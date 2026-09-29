# Phase 6: Resource Editor: portable port of editor.exe - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-09-30
**Phase:** 06-resource-editor-portable-port
**Mode:** Smart discuss, run autonomously. Johannes asked for the recommended answer every time and no questions back, so every choice below is the recommended one.
**Areas discussed:** Architecture and reuse; projects, export and data; previews and editing views; scope, verification and delivery (with the plan split).

---

## Architecture and reuse

| # | Question | Chosen (recommended) | Rejected alternatives |
|---|---|---|---|
| 1 | One binary with modes or a separate executable? | Separate `ResourceEditor` beside `Game` and `MapEditor` (D-01). The two editors have different documents, panels and startup needs. A separate binary keeps each one's tiers and packaging independent while they share the kit. | A single `Editor` binary with a map mode and a resource mode: one large app whose failures affect both, and the map editor's M2/M3 work would conflict with this phase. Growing the map editor app itself: it is map-specific throughout (`view`, `panels`, `document`). |
| 2 | How much of the map editor is reused? | Extract a shared Zig editor kit (`Sources/editor/kit`) and move MapEditor onto it first, with its tiers green (D-02). | Copying the files into a second app: they would drift apart and every fix would be needed twice. Referencing the map editor's modules in place: this couples the resource app to map-specific modules and names. |
| 3 | A separate bridge library or the same one? | The same `EditorBridge`, with a new `resource_bridge.h` and `BkRes*` entry points sharing the lifecycle (D-03). | A separate `ResourceBridge` library, which would duplicate startup, mods, overlay and capture and risk the host-globals coalescing trap twice. |
| 4 | Where do the resource schema and export live? | The MFC tree items and export code, ported to a portable C++ `ResourceModel` with the same `operator&` serialisation (D-04). This is the code that produced the shipped `Data/`. | Rewriting the schema and export in Zig: about 40k lines re-derived by hand, with a high risk of drift in the project and stats formats. A data-driven schema file: it still needs the export code, and it adds a third representation. |
| 5 | Undo model? | A generic property-tree command set plus one before/after command per geometry edit (D-05). | Whole-document snapshots per edit: simple, but slow and memory-heavy for units with many frames, and it cannot merge gestures. No undo (MFC parity): rejected because the map editor set the bar that every edit is undoable. |
| 6 | Several projects of a kind open, or one per kind? | One per sub-editor kind, kept while switching (D-06), as in MFC. | Tabs with several projects per kind: this is a new feature that goes beyond parity. |

## Projects, export and data

| # | Question | Chosen (recommended) | Rejected alternatives |
|---|---|---|---|
| 1 | Project file format? | MFC project XML unchanged, all 21 extensions, unknown nodes preserved (D-07). | A new JSON or Zig project format with a converter: the existing projects (in the repo and in GOG mods) would need migrating, and MFC could no longer open them during the transition. |
| 2 | Saving and locking? | The map editor's safe save plus `.bak`, autosave and recovery, and MFC's `locked_<user>` lock with a warning (D-08). | Keeping MFC's `backup.tmp` only. Dropping the lock files: they are the parity behaviour for shared project folders. |
| 3 | Where does export write, and how? | Into the mod export root, with MFC's paths and names, staged and then moved into place; never into `Data/` (D-09). | Writing in place file by file as MFC did: a failure leaves a half-written resource. Exporting next to the project: the game would not find the result. |
| 4 | Settings storage? | `<UserRoot>resourceeditor/resourceeditor.cfg`, with no registry import (D-10). | Importing the MFC registry keys on first start: Windows-only, and useful to almost nobody. Sharing `mapeditor.cfg`: the two editors' settings would be mixed. |
| 5 | What does "the game reads it unchanged" mean? | Engine-reader field equality for stats; byte identity for `.san`, packs, `_h.dds`, icons and copies; a measured pixel tolerance for DXT; idempotent project save (D-11). | Byte identity for everything: impossible, because the checked-in `editor.exe` used a different DXT encoder than `NDxt`. A visual check only: it would miss stats drift. |
| 6 | Where do the goldens come from? | MFC `editor.exe` batch mode on win-home over in-repo projects, GOG projects (by path) and repo-owned generated fixtures; only repo-owned goldens are committed (D-12). | Rebuilding the MFC editor from `editor.vcxproj` for the goldens: the checked-in binary is what users have run. Using the shipped `Data/` as goldens only: most shipped resources have no project to re-export (the Import round trip covers them instead). |
| 7 | What about resources with no project file? | Implement the dead "Import XML" as Import from game data, and offer "Export stats only" for every kind (D-13). | Opening only project files (strict parity): nearly every shipped resource would be uneditable. Import as a separate tool: it belongs in the editor, where MFC already had the menu item. |
| 8 | MOD, PAK and Run game? | Port MOD settings; a native PAK writer checked by mounting its output; Run game through the map editor's test launch (D-14). | Shelling out to `zip`/`zip.exe`: a platform dependency. Launching the game with the user's own profile: phase 3 D-02 forbids that. |
| 9 | Batch mode? | MFC's command line plus `all`, using the data-only startup (no GPU), plus the dialog (D-15). | UI-only batch: CI could not run it. Dropping the `-os` re-save flag: it is a parity feature. |

## Previews and editing views

| # | Question | Chosen (recommended) | Rejected alternatives |
|---|---|---|---|
| 1 | How is the preview drawn? | As the window background through the overlay path, in a new empty preview scene built from a real export into a temp storage (D-16). | Off-screen render to an ImGui texture: the map editor deliberately avoided a new off-screen path (its D-29), so this would be new renderer work. A hand-written preview renderer in Zig: it would not show what the game shows. |
| 2 | When is the preview proven? | Spiked and measured in 06-01, on a mesh unit, a sprite object and a particle source, before any sub-editor depends on it (D-17). | Discovering renderer limits inside each sub-editor plan: this repeats the phase 3 yaw surprise. |
| 3 | How are overlays drawn? | ImGui draw lists from world-to-screen projections; picking in the core (D-18). | Engine `DrawRects`/`CreateVertices` as in MFC: the portable renderer path is different, and it would need new bridge drawing calls for each overlay kind. |
| 4 | Playback and picture options? | Run/Stop with the game timer, camera, model and season switches, live gamma, `gamma.cfg` kept (D-19). | Always-running animation: MFC had explicit Run/Stop, and scripted shots need a still frame. |
| 5 | 2D editors? | ImGui textures from the picture cache, markers as overlays (D-20). | Routing them through the engine scene: needless for flat images. |
| 6 | Layout and widgets? | Tree left, inspector right, preview centre, bottom dock for thumbnails and curves, one generic property inspector (D-21). | One hand-built panel per sub-editor: 21 sets of UI code instead of one inspector driven by the model's domain types. |

## Scope, verification and delivery

| # | Question | Chosen (recommended) | Rejected alternatives |
|---|---|---|---|
| 1 | The GUI editor, switched off in MFC? | Port it last (06-15), against the game's current UI XML, and prove it by the game loading an edited screen (D-22). | Leaving it out as "disabled in MFC": this contradicts Johannes's "all editors". Porting it against the old `.gui` format only: the game's UI XML has grown since (Lua screens, scaling). |
| 2 | Commands with no handler in MFC? | Recover the intent in the owning plan; implement it if clear, otherwise record "no behaviour in MFC" in PARITY with the reason; Help shows shortcuts and links the spec (D-23). | Dropping them silently. Inventing new behaviour for them without evidence in the code. |
| 3 | Platforms and CI? | Mirror the map editor's tiers and runners, with batch-based tiers wherever the engine C++ builds (D-24). | App tiers on Linux: those runners have no video device (measured in the map editor spec). |
| 4 | Packaging? | `stage.zig --resource-editor`, staged beside `Game` (D-25). | A separate editors package: the test launch expects the game beside the editor. |
| 5 | When is the MFC editor deleted? | Only in 06-16, after every parity row, the goldens and `test-resources-all` pass and the hand try is approved; the goldens stay (D-26). | Deleting each MFC sub-editor as its port lands: the MFC editor is one binary and remains the golden oracle until the end. Keeping MFC indefinitely: the roadmap requires deleting it. |
| 6 | How is the phase split? | 16 plans in 6 waves: oracle and spikes, then kit and model, then bridge and core, then app shell, then 10 sub-editor plans, then sweep and deletion (D-27). | One plan per sub-editor (about 26 plans, with too many tiny data-only plans). Four big plans by family: too large to verify, and they would block each other. |
| 7 | When is a sub-editor done? | Every one of its PARITY rows, with a golden, an import round trip, undo tests, a measured preview shot and an auto scenario (D-28). | "It opens and exports": too weak for "all features implemented". |
| 8 | Phase exit? | The testable list in D-29. | A hand try only. |

## Claude's Discretion

- Struct layouts, kit module names, preview debounce, the DXT tolerance value (which 06-01 measures), the fixture art generator, ImGui details, recovery naming, and parallel or sequential execution of wave 5.

## Deferred Ideas

- Features beyond parity (stats balancing views, mod diffs); a Linux app build; export straight into a PAK; removing `LegacyUiCompat.h` after phase 7.
