# Phase 5: Map editor M3: random map templates, minimap tools, full parity - Pattern Map

**Mapped:** 2026-09-30
**Files analyzed:** ~70 new or modified files (grouped by D-39's 11 plans below)
**Analogs found:** 65 / 70 (5 have no in-repo analog; see "No Analog Found")

All paths are relative to the worktree root `/Users/johannes/Projects/src/Blitzkrieg/.worktrees/map-editor-6` (branch `feat/map-editor-m2`). Every analog below was checked with `git ls-files` and is tracked source. There is no `AGENTS.md`/`CLAUDE.md`; the binding rules are in 05-RESEARCH.md "Project Constraints": never `std::min`/`std::max` (use `Min`/`Max` from `Misc/Tools.h`), never `zig fmt build.zig` (run `zig test tools/zig/build_hermeticity_test.zig` after editing it), test artefacts in `zig-out/local-test`, core is std-only, every edit is a command with do/undo, shipped data is read-only.

**Line numbers are from this worktree as of this run** (M2 has landed; phase 4's PATTERNS.md line numbers for `session.cpp`, `bridge.h`, `view.zig`, `editor.zig`, `panels.zig`, `auto.zig` are stale — re-grep before each plan).

**The one chain to clone for every new record/edit (unchanged from M2, proven twice):**

```
tool/panel -> commands.zig named command -> Editor.addX/editX (history.reserve, bridge call, record)
  -> bridge.zig VTable -> c_bridge.zig (toC/fromC + two-pass read) -> bridge.h C ABI
  -> bridge.cpp (Guarded :134, WellFormed, bounds) -> session*.cpp (snapshot AND working together)
  -> test: fake_bridge.zig + editor.zig tests | editor_bridge_test.cpp | map_file_test.cpp
```

For derived/compound edits (altitudes, Update Map, RMG) use the **token + edit log** variant instead of a core-owned record: the bridge derives, keeps before/after, hands out a token (`IEditRecord`, `session_vso.cpp:33-84`), and the core's `Command.edit` holds the gesture's tokens (`history.zig:55`).

## File Classification

Roles: bridge entry / core command / app panel / app tool / engine fix / engine call / build step / test. Data flow: who calls it and what it reads/writes.

### Plan 05-01 Foundations

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|---|---|---|---|---|
| `Sources/src/MapFile/MapOverlay.h`/`.cpp` (MOD: `SAltitudeUndo` region record beside `SPaintUndo`) | utility (overlay ops) | CRUD on in-memory map | itself: `SPaintUndo` `MapOverlay.h:124-130`, `Paint`/`UndoPaint`/`CaptureRegion` `:135-143` | exact |
| `Sources/src/EditorBridge/bridge.h`/`.cpp` (MOD: `BkEditorSetAltitudes` region primitive, `BkEditorNewMap`, save-format flag) | bridge entry | request-response | paint block `bridge.h:224-255`; `Guarded` `bridge.cpp:134-148` | exact |
| `Sources/src/EditorBridge/session.cpp` (MOD: `ApplyAltitudesInSession` beside `PaintIntoSession :1355`) | service | region edit + engine sync | `PaintIntoSession`/`PutRegionBack` `session.cpp:1355-1520`; `SPaintRecord` `session.h:105-111` | exact |
| `Sources/editor/core/bridge.zig` (MOD: VTable altitude/new-map entries) | model / interface | request-response | itself (M2 VSO entries) | exact |
| `Sources/editor/core/editor.zig` (MOD: `setAltitudes` token command, `newMap`) | core command | CRUD + undo | `paint` `editor.zig:361-381` (reserve-then-token) | exact |
| `Sources/editor/core/history.zig` (MOD: reuse `.edit` scope or new `.altitude` variant) | core command | event-driven (undo stack) | `.edit` tokens+scope `history.zig:48-55` | exact |
| `Sources/editor/core/tools.zig` (MOD: brush size 1–16 even) | app tool | event-driven | `Brush` `tools.zig:73-130` | exact |
| `Sources/editor/app/panels.zig` (MOD: status bar, title, File→New/Save-as entries) | app panel | request-response | `drawMenuBar :2120-2201`, `drawMapMenu :2221-2234` | exact |
| `Sources/editor/core/settings.zig` (MOD: `default_format` key) | config | file-I/O | `applyKey` `settings.zig:113-142`, `format :166-173` | exact |
| `05-PARITY.md` verification | doc | - | `04-PARITY.md` (phase 4's) | exact |

### Plan 05-02 Heights, Update Map, Instant Update, Fit To Grid, Fill, tile properties

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|---|---|---|---|---|
| `Sources/src/EditorBridge/session_terrain.cpp` or extension of `session.cpp` (NEW: heights apply, `FillEntireMap`, `UpdateMap` composite, `GenerateHeights` over `CHField`) | service | region edit + engine calls | `PaintIntoSession` `session.cpp:1355-1445`; `BuildVso` derive-then-store `session_vso.cpp:168-180` | role-match |
| `bridge.h`/`.cpp` entries (fill map, update map, generate heights; progress callback param) | bridge entry | request-response + streaming (progress) | `BkEditorPaint` `bridge.h:253`; `IProgressHook` adapter (see P9) | role-match |
| `Sources/editor/core/tools_heights.zig` (NEW: Heights tool, brush 2–16, level modes) | app tool | event-driven (drag gesture) | `Brush` `tools.zig:73-130` + `RoadsRivers` gesture tool `tools_vso.zig:56-75` | role-match |
| `Sources/editor/app/panels_m3.zig` or `panels.zig` (MOD: Heights panel — speed, ratio, level mode, generate buttons) | app panel | CRUD (commit-on-deactivate) | Sounds edit fields `panels.zig:3000-3022`; M2 slider-gesture `panels_m2.zig:108-115` | exact |
| Tile properties context menu | app panel | request-response | `BkEditorDescribeTile` `bridge.h:284-285` (already returns the name) | exact |

### Plan 05-03 Object filters, Filters Composer, Fields tool

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|---|---|---|---|---|
| `Sources/editor/app/panels_logic.zig` (MOD: filter model over `Data/Editor/filter.xml`) | utility | transform (XML word lists → palette predicate) | `isPlaceable`/`Placeable` filter (phase 4, `panels_logic.zig`); `BkEditorActionCommands` ini read `bridge.h:889-902` | role-match |
| Filters read path (`bridge.h` entry or `files.zig` read) | bridge entry | file-I/O (shipped, read-only) | `BkEditorActionCommands` `bridge.h:889-902` (Data/Editor file read precedent) | exact |
| `Sources/editor/core/tools_fields.zig` (NEW: polygon Add/Select/Edit keys) | app tool | event-driven (polygon gesture) | `RoadsRivers` pending-polyline `tools_vso.zig:4-15, 72-74` (press/right-back/Enter-finish/Esc-drop) | role-match |
| Fields application entry (`bridge.h`: `BkEditorApplyField`) | bridge entry | request-response (engine `FillTileSet`/`FillObjectSet`/`FillProfilePattern` + region records) | `BkEditorDrawEntrenchment` compound edit `bridge.h:1393-1404` | role-match |
| Filters Composer window | app panel | CRUD | `panels_m2.zig` `drawRoadsRivers :78-141` | exact |
| User filters file `<UserRoot>mapeditor/filter.xml` | config | file-I/O | recovery-folder/user-path rules `panels_logic.zig:446-475` | exact |

### Plan 05-04 Multi-selection, properties, links, direction wheel, damage

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|---|---|---|---|---|
| `Sources/editor/core/tools.zig` (MOD: `Selector` → multi-select set, rubber band, cycle) | app tool | event-driven | `Selector` `tools.zig:145-200`; `refind` `:48-57` | exact |
| Properties panel (per-kind fields from `SEditorMApObject`) | app panel | CRUD (commit-on-deactivate) | `commitSoundEdit` `panels.zig:3036-3054` + `drawRow` `panels_m2.zig:53-69` | exact |
| Overlay/bridge: `fHP`, `nScriptID`, `link.nLinkWith` edits | bridge entry | CRUD on records | `BkEditorSetObjectScriptID` `bridge.h:794` (04-09 precedent); `SAddObject` fields `MapOverlay.h:23-25` | exact |
| Link/unlink (garrison/tow/couple) | bridge entry | CRUD + `CheckForInserting` rules | `MapOverlay::FindReferences` cascade pattern `MapOverlay.h:54`; delete-cascade order `session.cpp` (04-02 pattern) | role-match |
| `Sources/editor/core/tools_damage.zig` or `tools.zig` Damage tool | app tool | event-driven | `Placer` one-press tool `tools.zig:132-143` | exact |
| Direction wheel in palette | app panel | request-response | placer `dir` state `tools.zig:134`; palette in `panels.zig` | exact |

### Plan 05-05 Players, Unit Creation Info, Check Map

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|---|---|---|---|---|
| Players add/delete (`bridge.h`: diplomacy-length change) | bridge entry | CRUD (vector resize + object re-map) | `BkEditorSetAIGeneralSide` side-count resize `bridge.h:1064-1067` (resize + undo-restore-size precedent) | exact |
| Unit creation records (`records.zig` kind + `session_records.cpp` fns) | core command / service | CRUD | `records.Kind` `records.zig:16-36`; `session_records.cpp:1-60` file contract | exact |
| Unit Creation Info panel | app panel | CRUD (commit-on-deactivate) | `panels_m2.zig` groups/start-commands panels | exact |
| Check Map (core-side checks + results panel + log) | core command + app panel | batch read + log write | `BkEditorTerrainMatchesEngine` walk-and-report `bridge.h:328`; results list → `panels_m2.zig` panel; log → `files.zig` user write | role-match |
| Check Map <2-control-point VSO flag | core command | read over `BkEditorVso` (`control_count` already exposed, `bridge.h:1157`) | `LongEnough` `session_vso.cpp:158-161` (the same rule, editor side) | exact |

### Plan 05-06 Layers and fire ranges

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|---|---|---|---|---|
| `bridge.h` layer toggle entries (`IScene::ToggleShow`, `IGFX::SetWireframe`, `ToggleAIInfo`) | bridge entry | request-response (engine renderer state) | engine targets verified: `Scene/Scene.h:503`, `GFX/GraphicsEngine.h:232`, `iMainInternal.cpp:341`; bridge entry shape `bridge.h:649-654` (`BkEditorSetMapType`, "no engine say, cannot be refused" style) | role-match |
| Layer state persistence + re-apply after open/new | config / app panel | file-I/O + event | `settings.zig` `applyKey/format`; re-apply hook beside `mapOpened` (panels.zig cache reload pattern) | exact |

### Plan 05-07 Minimap panel and images

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|---|---|---|---|---|
| `Sources/editor/app/minimap.zig` (NEW: CPU-side texture, Editor/Game modes) | app panel | streaming (per-frame draw + incremental refresh) | `pictures.zig` `Pictures` `:59-155` (request/pump/lookup texture cache); `markers.zig:121,425` (draw list) | role-match |
| Minimap click → camera move | app panel | request-response | `BkEditorSetCamera` `bridge.h:489`; screen-centre offset per MFC `MiniMapDialog.cpp` (RESEARCH) | exact |
| `bridge.h` `BkEditorCreateMiniMapImage` entry | bridge entry | request-response (engine static call on saved map) | `BkEditorSaveMap` doc pattern `bridge.h:90-113` (acts on a file, verifies); engine static verified `MapInfo_StaticMethods_MiniMapCreation.cpp:52` | role-match |
| Minimap terrain colours (per-tile average from `_h.dds`) | engine call | transform | `BkEditorTilePicture` `bridge.h:287-317` (already decodes the tileset `_h.dds` and caches it per session — extend, don't rebuild) | exact |

### Plan 05-08 Create Random Map + Export lists

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|---|---|---|---|---|
| `bridge.h`/`bridge.cpp` `BkEditorCreateRandomMap` (+ `BkEditorListRmg` folder scans for combos) | bridge entry | request-response + streaming (19-step progress) | call-site contract `random_missions_test.cpp:179-264`; `Guarded` `bridge.cpp:134-148` | role-match |
| `session_rmg.cpp` or `bridge.cpp` block: `IProgressHook` adapter from C callback | service | streaming (C callback → engine interface) | `IProgressHook` `StreamIO/ProgressHook.h:3-11`; vtable-mirror precedent `StreamIOZig/legacy_bridge.cpp:143-153` (EditorBridge compiles against real engine headers, so implement the real interface, not a mirror) | role-match |
| Create Random Map dialog | app panel | request-response | `PathSlot` file dialogs + `panels_m2.zig` panel body; MFC fields `CreateRandomMapDialog.cpp` (RESEARCH) | exact |
| D-05 `szMODName` fix | engine fix | one line + comment | bug site `MapInfo_StaticMethods_RMGeneration.cpp:971`; M1 mod-stamp precedent `BkEditorSaveMap` doc `bridge.h:105-112` | exact |
| Determinism test (`tools/zig/` new or extension) | test | batch (generate twice, byte-compare) | recipe `random_missions_test.cpp:233-261` (seed restore + regenerate) — but D-40.5 wants byte-compare, not `AreEquivalent` | role-match |
| Export lists (Tools 0–3) | core/app command | file-I/O (write `*_list.txt` to `<UserRoot>mapeditor/logs/`) | line format from `MainFrm.cpp:939-1138` (MFC, port sources); user-folder write like recovery copy | role-match |

### Plan 05-09 Containers + Graphs composers, user RMG root

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|---|---|---|---|---|
| `bridge.h` RMG record I/O (list/scan/read/write through RandomMapGen serialisers) | bridge entry | CRUD + file-I/O through `LoadDataResource`/`SaveDataResource` | `LoadDataResource( pMission->szTemplateMap, ..., RMGC_TEMPLATE_XML_NAME, randomMapTemplate )` `random_missions_test.cpp:153, 169` (the exact read pattern) | exact |
| `session_rmg.cpp` (NEW: composer records, patch copy-in) | service | CRUD + storage resolve | `session_records.cpp:1-60` (file contract); `BuildVso` storage probe `session_vso.cpp:171-178` (`pStorage->IsStreamExist`) | role-match |
| Containers/Graphs composer windows | app panel | CRUD (tables + properties + context menus) | `panels_m2.zig` `drawRoadsRivers :78-141`; `drawRow :53-69` | exact |
| Graph canvas (nodes/links, Ctrl+drag link, drag move/resize) | app panel | event-driven + draw list | `markers.zig:121, 196, 267` (background draw list, `worldToScreen() orelse continue`); input gestures `tools_vso.zig` | role-match |
| User RMG storage root mount (D-09) | engine call / bridge entry | file-I/O (storage layer mount) | `BkEditorSetMod` mount pattern `bridge.h:410-435` (MOD storage swap + FilesInspector re-inspect); mount point in `BkEditorStart`/`BkEditorSetMod` (05-CONTEXT Integration Points) | role-match |
| Shipped-RMG read-only rule | config | file-I/O check | `shipped.zig` `isShipped` `:143-168`; `needsSaveAs` `panels_logic.zig:467-475` | exact |

### Plan 05-10 Fields + Templates composers

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|---|---|---|---|---|
| Fields/Templates composer windows | app panel | CRUD | `panels_m2.zig` panels; MFC field mapping `RMG_Types.h:327-370` (RESEARCH, verified) | exact |
| Composer Check! validations | core command | batch read + report | Check Map pattern (05-05); `nParts >= 8` rule matches engine clamp `RMGeneration.cpp:1455-1458` (RESEARCH) | role-match |
| Composer round-trip test (43+102+404+27 files) | test | batch (load→save→reload equal) | `random_missions_test.cpp` linking + step pattern `build.zig:6704-6798` (`addRandomMissionsTest`) | exact |

### Plan 05-11 App shell, parity verification, MFC deletion

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|---|---|---|---|---|
| Options (game parameters, default format) | app panel + config | file-I/O | `settings.zig` `Settings`/`applyKey`; Settings window `openSettingsWindow` `panels.zig:2164` | exact |
| View menu (show/hide panels, Reset layout) | app panel | request-response | `drawMenuBar` View block `panels.zig:2184-2193` | exact |
| Drag-and-drop map open, command-line map | app shell | event-driven + CLI | `main.zig` arg walk `:155-192` (`interactive(gpa, io, environ, arg, ...)` already opens a map path arg) | exact |
| Single instance (socket/pipe IPC) | app shell | event-driven (IPC) | no in-repo analog (MFC used `WM_COPYDATA`, Windows-only) | no analog |
| Help → Keys and tools, About | app panel | static | `panels.zig` window helpers `beginPanel :2433` | exact |
| MFC deletion (7 removal targets, D-38) | build step / removal | - | removal list in 05-CONTEXT D-38; `git grep` gate D-40.9 | no analog (removal) |
| `build.zig` `map-editor-m3-auto` step | build step | - | `map-editor-auto` `build.zig:6073-6111` + `map-editor-auto-m2` `:6120-6137, 6551` | exact |

---

## Pattern Assignments

### 1. Altitude region record: `MapOverlay.{h,cpp}` extension (utility, CRUD) — D-19

**Analog:** `SPaintUndo` and friends, `MapOverlay.h:112-143` (exact same problem: record a region so undo restores it raw).

```cpp
// MapOverlay.h:124-130 — the shape to copy for SAltitudeUndo
struct SPaintUndo
{
	CTRect<int> rPatches;								// the region, in patch coordinates
	std::vector<SMainTileInfo> tiles;		// row-major over the region's cell rectangle
	std::vector<STerrainPatchInfo> patches;	// row-major over rPatches
	SPaintUndo() : rPatches( 0, 0, 0, 0 ) {  }
};
// MapOverlay.h:137-143 — the deterministic function + raw undo
bool Paint( SLoadMapInfo *pMap, const std::vector<SPaintCell> &rCells, SPaintUndo *pUndo );
void UndoPaint( SLoadMapInfo *pMap, const SPaintUndo &rUndo );
void CaptureRegion( const SLoadMapInfo &rMap, const CTRect<int> &rPatches, SPaintUndo *pOut );
```

M3's altitude record: `SAltitudeUndo { CTRect<int> rVertices; std::vector<SVertexAltitude> altitudes; /* row-major, grown by the shade kernel ±1 */ }` with `SetAltitudes( pMap, rect, values, pUndo )` that (1) writes the heights, (2) runs `CMapInfo::UpdateTerrainShades` over the grown region, (3) captures the whole grown region. Keep the header's own rule verbatim: "Undo restores exactly these rather than re-running the function". The region MUST be grown by 1 vertex on each side before any capture (RESEARCH Pitfall 5, MFC `DrawShadeState.cpp:210-213`). Note `SVertexAltitude` is written raw with 3 padding bytes — always capture from a fresh read, never from a copied buffer (Pitfall 3).

**Tests:** extend `tools/zig/map_file_test.cpp` the paint way: apply over a region, `AreEquivalent` outside it unchanged, write-read-back byte-compare over the region.

### 2. Session region apply: `session.cpp` beside the paints (service) — D-19/D-20/D-22

**Analog:** `PaintIntoSession`/`PutRegionBack`/`UndoPaintInSession`, `session.cpp:1355-1520`, declared `session.h:287-291`; record struct `SPaintRecord` `session.h:105-111`.

```cpp
// session.h:101-109 — a paint is undone by putting back `before`, never re-run
struct SPaintRecord
{
	NMapOverlay::SPaintUndo before, after;
};
std::vector<SPaintRecord> paints;
```

Order of work to copy (`session.cpp:1355-1445`): capture engine state before (`engineBefore`) so a mid-way refusal puts it back raw → apply the deterministic function to the snapshot, then the working copy, undoing the snapshot if the second fails → push the region into the engine (`pTerrainEditor->Update`) → store the record → hand out the token. For **Update Map / Fill / Generate / Fields** (engine-composite commands), prefer the `IEditRecord` edit log over the paint stacks — it is the generalised form:

```cpp
// session_vso.cpp:33-41 — LogEdit: one implementation per edit kind, undo/redo by stored record
int LogEdit( SEditorSession *pSession, IEditRecord *pRecord )
{
	std::unique_ptr<IEditRecord> record( pRecord );
	pSession->edits.push_back( std::move( record ) );
	const int nToken = int( pSession->edits.size() ) - 1;
	pSession->appliedEdits.push_back( nToken );
	pSession->undoneEdits.clear();
	return nToken;
}
```

`UndoEditInSession`/`RedoEditInSession` (`session_vso.cpp:43-77`) enforce newest-first/redo-in-order and map to `BkEditorUndoEdit`/`BkEditorRedoEdit` (`bridge.h:1132-1133`) — M3's altitude/update/fill edits ride this exact log; the core's `Command.edit { tokens, scope }` (`history.zig:55`) already replays them.

### 3. Bridge entries: `bridge.h`/`bridge.cpp` (controller, request-response)

**Analog:** the paint block `bridge.h:224-255` (region struct + token out + refusal rules documented) and `Guarded` `bridge.cpp:134-148`.

```cpp
// bridge.cpp:134-148 — every entry point wraps its body; a throw becomes a status
template<class F>
BkEditorStatus Guarded( BkEditorSession *pSession, F body )
{
	if ( pSession == 0 ) return BK_EDITOR_NO_SESSION;
	try { pSession->szMessage.clear(); return body(); }
	catch ( ... ) { pSession->szMessage = "the engine threw"; return BK_EDITOR_FAILED; }
}
```

Conventions to keep (from `bridge.h`'s own doc comments):
- Structs: fixed `char[N]` buffers, ints for bools, floats for positions; document the unit in the field comment (`BkEditorPaintCell` `bridge.h:236`).
- Status vocabulary: `BK_EDITOR_BAD_ARGUMENT` = caller bug (null/unterminated/non-finite/bad index, checked `if`-guarded, never asserts); `BK_EDITOR_REFUSED` = ordinary no with `szMessage`; a refusal changes nothing (`bridge.h:127-135`).
- Two-pass reads: `out_count` always the total, short buffer = REFUSED after writing what fits (`BkEditorObjects` `bridge.h:203-218`).
- Token outs: `-1` after a refusal; token valid until the next open/close (`bridge.h:237-252`).
- Region/edit entries: `out_token` beside the payload (`BkEditorAddVso` `bridge.h:1194-1195`).
- New-map/save-format entries model their doc on `BkEditorOpenMap` `bridge.h:66-88` / `BkEditorSaveMap` `:90-113` (what changes, what is left, the read-back verification).
- RMG/composer entries take storage-relative names, never OS paths, and validate them as bare names the way `IsBareName` does (`session_vso.cpp:125-130`).

Register any new `session_*.cpp` in `build.zig`'s EditorBridge file list (the M2 block, ~`:3806-3809` — re-grep; never `zig fmt`).

### 4. Core command + token replay: `editor.zig`, `history.zig`

**Analog:** `Editor.paint` `editor.zig:361-381` — reserve-then-bridge-then-record with token merging:

```zig
// editor.zig:361-381 — the M3 altitude/update/fill commands copy this shape
pub fn paint(self: *Editor, cells: []const PaintCell, gesture: u32) EditError!void {
    if (cells.len == 0) return;
    if (self.mergeable(gesture, .paint)) |entry| {
        try entry.command.paint.tokens.ensureUnusedCapacity(self.allocator, 1);
        var token: i32 = -1;
        try self.noteOutcome(self.bridge.paint(cells, &token));
        entry.command.paint.tokens.appendAssumeCapacity(token);
        self.history.touchTop(self.allocator);
        return;
    }
    try self.history.reserve(self.allocator);
    ...
    self.history.recordAssumeCapacity(self.allocator, .{ .paint = .{ .tokens = tokens } }, gesture);
}
```

For altitudes reuse `.edit { tokens, scope }` (`history.zig:48-55`) and extend `EditScope` (`history.zig:13`) with an `altitudes` scope that bumps a terrain generation counter (panels compare `*_generation_seen`, the sounds pattern). One heights stroke = one gesture = one history entry, merged while the drag runs (`mergeable`/`beginGesture`, `editor.zig` — same file); Generate heights / Set Zero / Fill / Update Map commit with gesture 0 as single steps. Record-path edits (players, unit creation, properties' Script ID/HP) extend `records.Kind` (`records.zig:16-36`) and ride `record_edit`/`record_add`/`record_delete` unchanged. Import every new core file in `core/root.zig` (`refAllDecls`) or its tests never run.

### 5. Tools: `tools_heights.zig`, `tools_fields.zig`, multi-select Selector (controller, event-driven)

**Analog:** `Brush` `tools.zig:73-130` (stamp-once-per-cell, gesture, refused-stamp retry) for the heights brush; `Selector` `tools.zig:145-200` for multi-select (grab offsets, `catch |err| if (err != error.Refused) return err;` at `:177` — refused mid-drag is skipped, not fatal); `RoadsRivers` `tools_vso.zig:56-75` for the fields polygon.

```zig
// tools_vso.zig:4-15 (doc) + :72-74 — the polygon gesture the Fields tool copies:
// press adds, right press takes the last back, double-click/Enter finishes, Esc drops
pub const RoadsRivers = struct {
    kind: VsoKind = .road,
    ...
    pending: [max_pending]records.Vec3 = undefined,
    pending_len: usize = 0,
```

Tool contract: `handle(self, editor: *Editor, event: Event) EditError!void`; state only what a gesture needs; `Pointer` carries `world_x/y` AND `map_x/y` AND `tile` AND `screen_x/y` (`tools.zig:21`) — heights store map/tile units, polygon points world units like roads. Handle every `Event` variant explicitly (M1 tools' `else => {}` arms, `tools.zig:97`). Register in `tool_registry.zig` (`ToolId` `:17`, `entries` `:46` with `needs_right_button`/`needs_double_click`/`ctrl_click_is_right` flags — the registry already supports everything M3 needs) and add the tool's state field to `view.zig` (`:106-126`) + the `switch` in `dispatch`/`selectTool` (`:239`). Alt+drag as middle-drag (trackpads) is a view-layer mapping, not a tool change.

### 6. Panels: menu items, commit-on-deactivate, sliders (component, CRUD)

**Analogs:** menu bar `panels.zig:2120-2201` (`drawMenuBar`; Map menu `drawMapMenu :2221-2234` is where Fill/Update/Check Map/Unit Creation/Create Minimap Images go; Tools `:2175-2183` iterates `tool_registry.entries`; add the composers/Options beside it); commit-on-deactivate `panels.zig:3000-3054`:

```zig
// panels.zig:3000-3022 — every field followed by the pair; commit once on deactivate
_ = ig.igInputFloatEx("x", &edit.x, 0, 0, "%.1f", 0);
active = active or ig.igIsItemActive();
committed = committed or ig.igIsItemDeactivatedAfterEdit();
...
edit.active = active;
if (committed) commitSoundEdit(state, index);

// panels.zig:3036-3054 — commit = one editor edit + reload + re-take generation
fn commitSoundEdit(state: *State, index: usize) void {
    ...
    state.view.noteEditResult(editor, editor.editSound(index, record, 0));
    state.loadSounds();
    state.sounds_generation_seen = editor.sounds_generation;
}
```

The Properties panel (D-26) is exactly this with per-kind field sets; Script ID merges per typed value like the M2 script-ID field (one gesture per value). Sliders that should merge use the M2 slider-gesture trick `panels_m2.zig:108-115` (`if (ig.igIsItemActivated()) state.vso_slider_gesture = state.editor.beginGesture();`). M3 panels go in a new `panels_m3.zig` modelled on `panels_m2.zig` (head doc `:1-4`: "a thin ImGui layer over the named commands in commands.zig, so a button and a BK_EDITOR_AUTO `do=` run the same code"). Register windows in the View menu and give them layout entries. Pure logic (min/max clamps, unit conversion, expected-value builders for tests) goes in `panels_logic.zig` where `test-map-editor-panels` covers it.

### 7. Named commands: `commands.zig` (utility, request-response)

**Analog:** itself — `command_table`/`predicate_table` `commands.zig:30-120`, `run :130`.

```zig
// commands.zig:30-33 — M3 appends e.g. map_fill, map_update, heights_generate,
// check_map, minimap_create, rmg_open, composer_new/save, export_lists, layer_toggle
pub const command_table = [_]Entry{
    .{ .name = "camera_player", .handler = cameraPlayer },
```

Every M3 menu item, panel button and auto-scenario step calls a named command here (names `[A-Za-z0-9_]{1,32}`, args ≤64 printable-ASCII-no-space chars — `auto.zig:126-128`), so `map-editor-m3-auto` never needs ImGui text entry. `Outcome { ok, refused, unknown_name, bad_arg }` (`:24`) with the status line carrying the refusal words.

### 8. Minimap: `minimap.zig` (component, streaming) — D-14..D-17

**Analogs:** `pictures.zig` for the texture (`Pictures` `:59-155`: `request`/`pump`/`lookup`/`clear`, `SDL_CreateGPUTexture` + transfer-buffer upload `:157-223`, `Source` enum `:52-57` — add a minimap source or reuse the pattern directly); `markers.zig:121, 425` for draw-list drawing (`igGetBackgroundDrawList()`, `real.worldToScreen(x, y) orelse continue`).

- The live minimap is a CPU-side RGBA buffer refreshed incrementally after edits; upload through the same `upload` path (R8G8B8A8_UNORM, sampler usage). Terrain colours: per-terrain-type average sampled from the tileset `_h.dds` — the session already decodes and caches that texture for `BkEditorTilePicture` (`bridge.h:287-317`, cached "for the session, for the tileset last asked about"): expose the averages through a bridge entry rather than decoding again in Zig.
- Click/drag → camera: compute the world point, keep the MFC screen-centre offset, call `BkEditorSetCamera` (`bridge.h:489`); the panel reads `BkEditorViewState` (`:512`) for the camera frame corners.
- 17 player colours and fire-range drawing port from `MiniMapTypes.cpp` (RESEARCH anchors; MFC reference until plan 05-11 deletes it).
- Game mode reads `<map>.tga` / `_h.dds` — decode through the engine's image processor like object pictures, then the same texture cache.
- Create Minimap Images: explicit command → Save As if shipped/never-saved → `BkEditorCreateMiniMapImage(path)` → engine static `CMapInfo::CreateMiniMapImage` (`MapInfo_StaticMethods_MiniMapCreation.cpp:52`) writes `<map>_large` (512) and `<map>` (256) DDS+TGA next to the map — the bridge entry verifies the four files exist after, the `BkEditorSaveMap` read-back habit (`bridge.h:96-98`).

### 9. Create Random Map: bridge entry + progress adapter + determinism (D-01..D-05)

**Analogs (three, all verified):**

(a) The caller contract — `random_missions_test.cpp:179-264`:

```cpp
// random_missions_test.cpp:69-78 — the output root MUST be absolute, backslashes, trailing one
static std::string GeneratedRoot( const std::filesystem::path &directory )
{
	std::string sz = std::filesystem::absolute( directory ).string();
	for ( char &c : sz )
		if ( c == '/' )
			c = '\\';
	return sz + "\\";
}
// :203 — the call the bridge entry wraps
SRMUsedTemplateInfo used;
const bool bGenerated = CMapInfo::CreateRandomMap( pMission, c.szContext, c.nDifficulty, nGraph, nAngle, true, true, &used, 0, szRoot );
```

The bridge entry (`BkEditorCreateRandomMap`) takes: template/context/setting names (storage-relative, validated bare-ish), graph index (-1 any), angle (-1 any), level 0..2, bzm/dds flags, map name, optional seed (0 = random), the user/mod maps folder as `rszOutputRoot` (built exactly like `GeneratedRoot`), and a `void (*)(int step, void *user)` progress callback; returns seed used + `SRMUsedTemplateInfo`. It seeds `IRandomGen` first when a seed is given (the generator stores and re-seeds itself, `RMGeneration.cpp:901-922`). Main thread only (D-03); no cancel.

(b) The progress adapter — real interface `StreamIO/ProgressHook.h:3-11` (`SetNumSteps/Step/Recover/SetCurrPos/GetCurrPos/Stop`), 19 steps (`RMGC_CREATE_RANDOM_MAP_STEP_COUNT`). EditorBridge compiles against engine headers, so implement `IProgressHook` in `session*.cpp` as a plain C++ class forwarding `Step()` to the C callback; the vtable-mirror precedent `StreamIOZig/legacy_bridge.cpp:143-153` exists only because that file cannot include engine headers — do NOT copy the mirror here. The callback must not call back into the bridge (overlay rule, `bridge.h:563`): it sets a counter the app's modal reads, or pumps the platform message loop inside `Step()` exactly as MFC's `CCreateRandomMapProgress` does (RESEARCH Pitfall 4).

(c) The determinism recipe — `random_missions_test.cpp:233-261`: copy `first.bzm` (native separators for `std::filesystem`, `:247-253`) → `CreateObject<IRandomGenSeed>(STREAMIO_RANDOM_GEN_SEED)` → `pSeed->Restore(seedStream)` → close the stream before regenerating (Windows share modes, `:242`) → `GetSingleton<IRandomGen>()->SetSeed(pSeed)` → regenerate with the SAME graph index and angle (from `SRMUsedTemplateInfo`) → compare. **D-40.5 wants byte-identical `.bzm`, stricter than the tier's `AreEquivalent`** — byte-compare the two files.

After generation the editor opens the map as a normal document (`BkEditorOpenMap`) with snapshot/preservation as any open (D-02).

### 10. D-05 engine fix: `szMODName` (engine fix, one line)

**Analog/site:** `MapInfo_StaticMethods_RMGeneration.cpp:971` — `mapInfo.szMODName = randomMapTemplate.szChapterName;` stores the chapter name. Fix: record the active mod's name/version the way the M1 save stamps them (`BkEditorSaveMap` doc `bridge.h:105-112`). The fix changes only what the generated map carries; the game's own callers (`RandomMapHelper.cpp:75`, `Mission.cpp:169`) pass their own mission stats and must keep working — the random-missions tier re-checks them.

### 11. RMG composers: serialisers, scanning, storage root (D-06..D-12)

**(a) Format fidelity:** never re-type the formats in Zig. Read/write through the RandomMapGen serialisers behind bridge entries:

```cpp
// random_missions_test.cpp:152-153 — the load pattern for every composer record
SRMTemplate randomMapTemplate;
if ( !LoadDataResource( pMission->szTemplateMap, "", false, 0, RMGC_TEMPLATE_XML_NAME, randomMapTemplate ) )
```

`RMGC_*_XML_NAME` labels (`"Template"`, `"Container"`, `"Graph"`, `"FieldSet"`, `"ChapterSetting"`…) are in `RMG_Consts.cpp`; struct+serialiser anchors in `RMG_Types.h` (RESEARCH table, verified). Saves go through `SaveDataResource` and must round-trip load→save→reload equal on all 43/102/404/27 shipped files (D-40.4). The `QuickLoadMapInfo` entry rides the template save (chunk 2, `MapInfo_Consts.cpp:28-29`).

**(b) Lists by scanning storages** (D-08, not `Editor\Default*.xml`): a bridge entry that walks the mounted storages' folders (`Scenarios/Containers`, `Graphs`, `FieldSets`, `Templates`, `Settings`) — the `BkEditorMods` directory-scan shape (`bridge.h:397-408`: sorted, missing dir = empty not error) and `StdFiles.list`'s `dir.iterate()` pattern (`files.zig`, M2 04-01) are the models.

**(c) User RMG root (D-09):** mount `<UserRoot>rmg/` (or `<UserRoot>mods/<Folder>/rmg/`) as a storage layer over Data and the mod, in `BkEditorStart`/`BkEditorSetMod` beside the MOD storage mount — the mount/swap sequence is documented on `BkEditorSetMod` (`bridge.h:410-435`): close open map, swap storage, FilesInspector re-inspect, rebuild DB. User-authored saves go under that root mirroring the `Data` layout; shipped RMG files are read-only through `shipped.zig` `isShipped :143-168` + `panels_logic.needsSaveAs :467-475` (Save becomes Save As).

**(d) Patch copy-in (D-10):** when a chosen patch is not reachable through the mounted storages, offer to copy it into `<rmg root>/Scenarios/Patches/<season>/` — replaces the MFC refusal (`RMG_CreateContainerDialog.cpp:207`). The storage probe is `pStorage->IsStreamExist` as in `BuildVso` (`session_vso.cpp:171-178`); the copy follows the test-launch script-copy precedent (app-side copy after `BkEditorTestMapPath` validates, phase 4).

**(e) Graph canvas (D-11):** ImGui draw-list canvas; node/link hit-testing and drag/resize gestures follow the M2 width-handle/control-point hit radii pattern (`tools_vso.zig:44-54` documents the MFC radii convention); overlap-reverts and Ctrl+drag links are new gesture logic in the composer's state struct (the `RoadsRivers` pending/selected split is the model). `nParts >= 8` validation matches the engine's own clamp.

### 12. Layers (D-32) and settings persistence

Engine targets (verified tracked): `IScene::ToggleShow` `Scene/Scene.h:503` (+ `SceneInternal.cpp:246`), `IGFX::SetWireframe` `GFX/GraphicsEngine.h:232` (+ `iMainInternal.cpp:341`), `ToggleAIInfo` (AI editor). Bridge entries follow the `BkEditorSetMapType` shape (`bridge.h:649-654`): renderer state, no map data, cannot corrupt anything. The state is re-applied after every open/new (fixes the MFC desync) — hook beside the panels' `mapOpened` cache reload, and persist the toggles in `settings.zig` with a new key in `applyKey`/`format` (`:113-173`): the lenient `key=value` parser already ignores unknown keys, so adding keys is backward-compatible. Measure Wireframe/Depth-Complexity on GFXGPU headless first (RESEARCH Open Question 1, PARITY L3/L4).

### 13. Check Map (D-33) and the railroad guard (carried bug)

- Checks run core-side over existing reads: duplicates (position + `szKey` + frameIndex), links (`FindReferences` `MapOverlay.h:54` is the reference oracle), player index, parties, unknown object types (M1's `unknown_object_count`, `bridge.h:64`), and road/railroad `<2` control points — `BkEditorVsoInfo.control_count` is already exposed (`bridge.h:1157`); the rule is `LongEnough` (`session_vso.cpp:158-161`) with `< 2` flagged as an error. Results panel rows jump to the entry (`markers.zig`-style world→screen); log to `<UserRoot>mapeditor/logs/checkmap_log.txt` (user-folder write, recovery-copy pattern). "Fix all" is one undoable command built from the same edit calls.
- **Engine guard** (the game must never crash): skip railroads with `controlpoints.size() < 2` in `CRailroadGraphConstructor::Construct`'s filter loop:

```cpp
// RailroadGraph.cpp:624-630 — the site (guard goes on the loop's condition)
void CRailroadGraphConstructor::Construct( const STerrainInfo &terrain, CRailroadGraph *pGraph )
{
	for ( int i = 0; i < terrain.roads3.size(); ++i )
	{
		if ( terrain.roads3[i].eType == SVectorStripeObjectDesc::TYPE_RAILROAD )
			railroads.push_back( new CRailroad( terrain.roads3[i] ) );
	}
```

Crash chain for the plan's rationale: `CSplineEdge` ctor reads `controlpoints[0]` unconditionally (`RailroadGraph.cpp:55`) and `edgeParts[nControlPointsSize-1]` = `edgeParts[-1]` when empty (`:69`); called from `CAILogic::Init` (`AILogicInternal.cpp:820-821`). Prove with a game-reads-it scenario (`BK_AUTO_UI` clean exit) — the editor's headless session does not run `CAILogic::Init`.

### 14. App shell (D-34) and single instance

- Command-line map open already exists: `main.zig:155-192` walks args and passes a non-flag arg to `interactive(...)` as the map. Drag-and-drop: map SDL's drop events onto the same open path as the File dialog (through the unsaved-changes guard).
- Options: game command-line parameters feed `testlaunch.zig` (which builds the game's argv); default format goes to `settings.zig` and `BkEditorSaveMap`'s extension rule (`bridge.h:90` — format comes from the extension, so the app picks the temp path's extension).
- Title/status bar: `drawMenuBar :2120` neighbourhood; title = name + ext + `*` (`history.dirty()`) + size in patches (`BkEditorMapSummary` `bridge.h:52-64`) + mod key (`BkEditorActiveMod` `:440`).
- **Single instance:** no in-repo analog (see No Analog Found). Per-user Unix domain socket / Windows named pipe at the app's discretion; second launch sends its path, exits 0; the running instance's loop polls the pipe on its existing event pump.
- Help/About: plain `beginPanel` windows (`panels.zig:2433`).

### 15. Auto verbs and the M3 scenario (D-37/D-40.7)

**Analogs:** `auto.zig` `Action` union `:130-154` (add nothing unless a feature cannot be reached through `do=`/`expect=` — commands.zig is the preferred surface), parser `parse/parseAction :197-232` with per-case `fail(...)` messages; runner verbs in `smoke.zig`. The `map-editor-m3-auto` build step clones `map-editor-auto` (`build.zig:6073-6111`: `--hidden` + staged shipped map + `BK_EDITOR_AUTO` env + `has_side_effects = true` + `dependOn(&smoke_run.step)` + delete-matching-files cleanup) but scripts through named commands like the M2 step (`build.zig:6120-6137`: a Zig array of `frame:do=name` entries joined with commas, one block of lines per plan). New test executables follow `addRandomMissionsTest` (`build.zig:6704-6798`): staged beside Game (loader-relative roots), `@executable_path` rpath, Windows `mainCRTStartup`, same link set.

### 16. Test tiers (D-37/D-40)

- **Core:** `fake_bridge.zig` — one `ArrayListUnmanaged` + `CallKind` + seed fixture + vtable entries per new collection; every new `Bridge.VTable` field must appear in the fake and `c_bridge.zig` vtables or Zig fails to compile (the intended net). Editor tests copy the add/undo/redo/refused-unchanged quartet (`tools.zig:216-255` shapes).
- **Map-file:** `tools/zig/map_file_test.cpp` — `Check()` harness, `const CMapInfo original = map;`, `AreEquivalent( before, after, &szWhere )`, write-read-back byte compare; expected maps built with the same functions (D-19's builder = `SetAltitudes` itself).
- **Engine:** `tools/zig/editor_bridge_test.cpp` — `TestSoundList` shape (read, add/set/delete, save+read-back, refusal block, `AreEquivalent` tail, `remove` scratch); add every new entry point to the null-session/no-map tables.
- **Engine Zig:** `c_bridge_test.zig` — `expectEngineMatches` after every step, do/undo/redo/undo-all, ends `!editor.dirty()`.
- **Data-only:** the composer round-trip and determinism tools link like `addRandomMissionsTest` and run on the five engine-C++ targets.

---

## Shared Patterns

### Two copies, one edit (snapshot + working)
**Source:** `session.h:40-41`, `session.cpp` sound/paint/record functions. Every M3 session function edits `pSession->snapshot` and `pSession->working` in the same function; the snapshot is saved, the working copy feeds the engine. `SaveSessionMap` (`session.cpp:410`) writes the snapshot only, with read-back + `AreEquivalent` verification.

### Derive once, undo by stored record (the token discipline)
**Source:** `SPaintRecord` `session.h:105-111`; `IEditRecord` + `LogEdit` `session_vso.cpp:33-84`; `history.zig` `.edit`. Applies to: altitude strokes, Update Map, Fill, Generate, fields application, minimap image writes (no undo), composer file writes (file-level, no map history). Undo/redo never re-run a deterministic function.

### Reserve before the bridge commits
**Source:** `editor.zig:361-381`, `history.reserve` + `recordAssumeCapacity`. A command's recording can never fail after the bridge acted; a refusal leaves history, document and generations unchanged (assert in tests).

### Status vocabulary and units
**Source:** `bridge.h:14-23` (codes), `:127-150` (edits contract), `:618-647` (units). Map (AI) vs world (Vis) vs VIS-tile units documented per field; conversions in the bridge, never the core; `Min`/`Max` not `std::min`/`std::max`; Hungarian prefixes; `StdAfx.h` first include.

### Guards not asserts
**Source:** `bridge.cpp:134-148`, `session_vso.cpp` `LongEnough`. Engine asserts are compiled out in release; every index/size/empty/non-finite check is an `if` returning a status.

### Shipped is read-only; user data under `<UserRoot>`
**Source:** `shipped.zig` `isShipped :143-168`; `panels_logic.needsSaveAs :467-475`; `BkEditorTestMapPath`'s bare-name validation `bridge.h:464-470`. Applies to shipped RMG files (D-09), minimap/log outputs (`<UserRoot>mapeditor/logs/`, `filter.xml`), and the RMG output root (user/mod maps folder, never `Data`).

### Generation counters + cache reload
**Source:** `sounds_generation` `editor.zig:41`, `loadSounds`/`sounds_generation_seen` `panels.zig:3036-3054`. One `u32` per collection, bumped in every edit and replay; panels re-read when it moves. Add `altitudes_generation`/`layers` equivalents.

### Named commands are the only UI logic
**Source:** `commands.zig:1-9, 30-130`. Every M3 button/menu item/auto step calls `commands.run`; panels stay thin.

### Build wiring
**Source:** EditorBridge/MapFile `.files` lists in `build.zig` (M2 block ~`:3806`); after any `build.zig` edit run `zig test tools/zig/build_hermeticity_test.zig`; never `zig fmt build.zig`. Test steps: existing registry at `build.zig:2838, 5885+, 6063-6111, 6120-6137, 6551, 6587, 6704`.

### Tests must be imported to run
**Source:** `core/root.zig` (`refAllDecls`). New core files into `root.zig`; new pure app logic into `panels_logic.zig`/`auto.zig` roots; confirm test names in step output.

---

## No Analog Found

| File/Feature | Role | Data Flow | Reason / What to do |
|---|---|---|---|
| Single-instance IPC (`Sources/editor/app/single_instance.zig` or in `main.zig`) | app shell | event-driven (IPC) | Nothing in-repo does IPC (MFC used `WM_COPYDATA`, Windows-only, `MainFrm.cpp:1200`). Agent's discretion (D-34): per-user Unix domain socket / named pipe; poll on the existing SDL event loop; treat a blocked peer as "not running" and start normally. |
| `IProgressHook` C-callback adapter | service | streaming | No EditorBridge code has driven a progress interface before. Implement the REAL interface (`StreamIO/ProgressHook.h:3-11`) inside `session*.cpp` (the bridge includes engine headers); `StreamIOZig/legacy_bridge.cpp:143-153` is a precedent only for "tool code driving this interface", not a pattern to copy. Follow Pitfall 4 (no bridge re-entry from the callback). |
| Minimap CPU rasteriser + incremental refresh | app panel | streaming | `pictures.zig` covers decode/upload; nothing refreshes a texture from map data incrementally. Design: per-tile colour grid (bridge entry averaging `_h.dds` per terrain type, reusing `BkEditorTilePicture`'s cached texture), dirty-rect rebuild on `*_generation` bumps, full rebuild on open/new/mod. Tick rate is the agent's discretion (CONTEXT). |
| Graph canvas node/link interactions (D-11) | app panel | event-driven | The M2 tools hit-test points on polylines; no node/rect graph editing exists in Zig. Port interactions from `RMG_CreateGraphDialog.cpp:1061-1548` (MFC reference, deleted in 05-11 — port before then); overlap-revert and Ctrl+drag link are new gesture code over the `tools_vso.zig` state pattern. |
| MFC editor deletion (05-11) | removal | - | Not a pattern — a checklist (D-38's 7 targets) with a `git grep` gate (D-40.9) and the full CI matrix as proof. No in-repo removal precedent at this scale. |

## Metadata

**Analog search scope:** `Sources/src/EditorBridge` (all), `Sources/src/MapFile` (headers + overlay), `Sources/editor/core` (history/tools/settings/shipped/records/editor/tools_vso), `Sources/editor/app` (panels/panels_m2/commands/auto/pictures/markers/main/view/tool_registry), `tools/zig/random_missions_test.cpp`, `Sources/src/RandomMapGen/MapInfo_StaticMethods_RMGeneration.cpp`, `Sources/src/AILogic/RailroadGraph.cpp`, `Sources/src/StreamIOZig/legacy_bridge.cpp`, `Sources/src/{Scene/Scene.h,GFX/GraphicsEngine.h,Main/iMainInternal.cpp}`, `build.zig` step registry, phase 4's `04-PATTERNS.md` + plans.
**Files scanned:** ~30 read in whole or in ranges (largest: `bridge.h` full; `session.h`, `session_vso.cpp`, `panels.zig`, `panels_m2.zig`, `commands.zig`, `records.zig`, `settings.zig`, `shipped.zig`, `pictures.zig`, `tools.zig`, `history.zig`, `auto.zig`, `editor.zig`, `main.zig`, `MapOverlay.h`, `random_missions_test.cpp`, `build.zig` step ranges, engine seam ranges).
**Tracked-source gate:** all analog paths verified with `git ls-files` (11/11 spot-check non-empty); the untracked `.gsd/` mirror was excluded; no capability-mirror paths emitted.
**Pattern extraction date:** 2026-09-30
**Line-number validity:** current as of this run on `feat/map-editor-m2`. Plans 05-02..05-11 share `bridge.h/bridge.cpp`, `session.cpp`, `editor.zig`, `panels.zig`, `view.zig`, `auto.zig`, `commands.zig`, `build.zig` — re-grep anchors before each plan starts; waves land in order (1 → 2-8 parallel → 9 → 10 → 11) and shift line numbers as they do.
