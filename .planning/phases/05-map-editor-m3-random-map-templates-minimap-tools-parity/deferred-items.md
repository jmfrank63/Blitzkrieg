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
  **Resolved (fix/m3-fields-flake, 9f19fb411):** not a scanline effect - the tile variants come from
  the unseeded C runtime rand() (`STileTypeDesc::GetMapsIndex`); `SeedFieldFills` now seeds it and
  the test compares the tile values exactly. Window 4 is fixed.

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
  **Resolved (fix/m3-fields-flake, 9f19fb411):** the two seeded applies now save the same bytes
  and the test asserts it.

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

## From 05-06

- **Depth Complexity is greyed, not drawn.** The probe measured the GPU renderer painting the whole frame white
  for `SCENE_SHOW_DEPTH_COMPLEXITY`: the D3D path counts overdraw in the stencil (SceneDraw.cpp:731-741 over effects
  300, 301 and 310-329) and the GPU renderer's stencil has one mode (`effects.StencilMode.darken_once`) and no
  counter. A real counter needs an increment-and-wrap stencil op, a per-draw stencil reference and the 20 colour
  rects keyed on it in `Sources/src/GFXGPU/renderer.zig`; until then the bridge's mask leaves the layer out
  (`LayerAvailableMask` in `session_layers.cpp`) and the menu greys it with the finding. When the renderer learns it,
  the mask line goes and `TestM3Layers`' pinned mask with it.
- **A filter made in the Filters Composer and not saved cannot drive the fire ranges.** The bridge validates the
  filter name against the filter files (shipped and user), as 05-06's plan says; the composer's live list also holds
  names not saved yet. Choosing one answers "no object filter is named ..." and the mode that showed stays. Saving the
  filters first makes it work.
- **The fire-range filter's combo is a submenu, not the MFC toolbar combo.** Layers > Unit Fire Ranges lists Off,
  Selected units and every filter; the MFC's combo sat in the toolbar beside the fire-range button.
- **The wire frame draws the sprites as outlines too.** D3D's `D3DRS_FILLMODE` did the same; the ImGui overlay draws in
  its own pass and is not affected.
- **The Windows leg** (`test-editor-bridge`'s layer tests, `map-editor-m3-auto`'s shot comparisons, the wire-frame pipeline
  on D3D12/Vulkan through SDL GPU) waits for the orchestrator's push: a GUI scenario cannot be launched over
  `ssh win-home`. All of it ran green on macOS arm64.

## From 05-08

- **A generated map names the script by its absolute path.** The engine's `CreateRandomMap` sets
  `mapInfo.szScriptFile = szRandomMapName` - the output path with the folder in it - as it did in the MFC editor
  (Data root) and as the game's briefing relies on (the generated-data root). A map generated into
  `<UserRoot>maps` therefore records `<UserRoot>maps\<name>`; `Test in game` then says the script's name "is not a
  plain name, so it was not copied" and the game starts without the secure-area script, although `<name>.lua`
  sits beside the map. It also makes two generations of one seed into two folders differ in exactly that path
  (found by the determinism harness; it generates into one folder and copies the first aside). Not fixed here:
  the field is the engine's and the game's own callers read it - the editor's fix is to put the bare name there
  after generation, which is a script-name edit of a freshly written file (05-11's hand try should look at it).
- **The Settings window's "Maps folder" does not move a generated map.** The generator appends `maps\<name>` to
  an output root, so the bridge builds the root from the user root (or the mod's folder) - D-17's default maps
  folder - and a custom folder typed into Settings is not honoured for generation (the map still opens normally).
- **The progress modal does not animate while the generator runs.** The window waits for the whole generation
  (D-03's accepted model, the Update Map one): the modal is on screen at 0 of 19 for the frame before the call and
  shows 19 of 19 with the seed after it. The callback only counts - never pumps events - so the overlay rule holds.
- **Windows.** The generation, the determinism harness and the m3-auto frames wait for the orchestrator's push and
  05-11's win-home run (no GUI over SSH); on Windows the m3-auto run's generated map and lists land in the
  profile's own `%APPDATA%\Nival\Blitzkrieg` (the macOS/Linux run redirects XDG_DATA_HOME to
  `zig-out/local-test/map-editor-m3-auto-user`).
