# Phase 5 deferred items

Out-of-scope discoveries logged by the executors (not fixed in the plan that found them).

## From 05-04

- **Older engine tests leave scratch files behind on macOS.** The pre-05-04 tests of
  `tools/zig/editor_bridge_test.cpp` call `remove()` on scratch paths joined with `"\\"`
  (the engine's own separator), which POSIX `remove()` does not split, so the files stay in
  `zig-out/local-test`. 05-04 routed its own three tests (`TestM3MultiSelect`,
  `TestM3PropertiesAndLinks`, `TestM3Damage`) through `OsPath`, as the M2 tests already do;
  the rest are untouched (harmless: the folder is the tests' own scratch space).
- **Ctrl+click and Alt+click are not scriptable in BK_EDITOR_AUTO.** `click=` carries no
  modifiers, so the m3-auto scenario reaches the Ctrl rubber band through `band_select` and
  the Damage tool's repair through `damage:<object>:repair`; the gestures themselves are
  proven by the core Selector tests and the view test
  `the Damage tool - left damages, right heals, Alt+left and the middle button repair to full`.
- **The direction wheel's hot area is the filter row's height.** The dial is drawn 40 px
  tall beside the palette's filter input but the invisible button is one frame high, so
  only its upper half takes a drag (kept so the M1 reference frames' row heights hold).
- **The flag swap's engine re-placement.** `BkEditorSetObjectFields` renames a re-owned
  flag's record (Flag_<party>) but the engine object keeps its old type until the map is
  reopened; the record (what is saved) is right.
- **TestM3Fields' touched-cell proof flakes.** One run of `test-editor-bridge` during 05-04
  printed `the fields' tiles change exactly the cells the engine's own fill touches (63
  touched, 1 disagree)` - the final-scanline replay gap already open in `.planning/WINDOWS.md`
  (entry 4, from 05-03); the next runs passed.

## From 05-05

- **Appear points are placed from a list, not by a map click.** The Unit Creation Info window
  lists a player's appear points in tiles (the MFC's own points dialog shows map units / 64) and
  adds one at the view centre; a hidden "click on the map" tool as the Start Target tool has was
  not built - the MFC editor has none for appear points either (PEPointsListDialog.cpp:107-130,
  typed coordinates only).
- **A party change does not rename the flags.** The MFC's `ResetPlayersForFlags` re-types every
  flag of the map when a party is set (UnitCreation.cpp `SetPartyName`); the editor never edits an
  object it was not asked to, so existing flags keep their names until their owner moves
  (properties' flag swap, a player delete) - the player edits do rename a flag that changes owner.
- **A shared link ID is reported by Check Map, not fixed.** The MFC deleted both records
  (TEF:6078-6144); the bridge keeps every edit away from a record whose link ID other records
  share (RefuseSharedLinkID), so Fix all counts it as left.
- **The Windows leg of the new tiers** (`map-editor-game-reads-it-m3`, `map-editor-m3-auto`, the
  engine tests) waits for the orchestrator's push; all of it ran green on macOS arm64.
- **`TestM3Fields`' byte diagnostic** (`editor-bridge: (identical? ...)`) printed again in the
  gate runs (the known window 4); the test passed.

## From 05-07

- **The minimap's markers are not filtered by the object palette's filter.** The MFC's
  `CUnitsSelection::Update` asked the palette's `FilterName` for every object (and for the one
  squad-marker gate `squads\german_hmg`), so choosing a filter such as "Buildings" thinned the
  minimap. `BkEditorMinimapUnits` answers every object the database knows; the panel draws them all.
  The D-31 filters are matched on the palette's catalogue paths app-side, so wiring them in is an app
  change (the markers would need their object's name or path), not a bridge one.
- **The Heights ramp does not mark invalid heights red.** The MFC painted a vertex the engine's
  `IsValidHeight` refuses in red; no read says which those are, so the ramp is grey only.
- **The fire-range areas come from the AI, not from a Layers toggle.** `BkEditorMinimapAreas` reads
  what `IAILogic::UpdateShootAreas` shows now. Until plan 05-06's fire-range layer registers a group and
  calls `ShowAreas`, nothing shows them; the engine test registers one itself. 05-06 needs no minimap work.
- **The minimap rebuilds the whole texture on any document change.** A dirty flag (the history's
  revision, the Heights tool, the map's size), at most once a frame, not a dirty rectangle; a paint
  stroke on a 512x512 map re-reads and re-uploads the tiles each frame it moves. Fast enough in the
  scenario's maps; a region read per edit is the next step if a big map stutters.
- **The Windows leg** of `test-editor-bridge` (the minimap tests), `test-map-editor-engine` and
  `map-editor-m3-auto` (the shot comparison) waits for the orchestrator's push: a GUI scenario cannot be
  launched over `ssh win-home`. All of it ran green on macOS arm64.
