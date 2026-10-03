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
  **Resolved (05-11):** the row's button stays one frame high and holds the layout; a second
  invisible button, 40 px square over the dial, takes the pointer (the row's button allows the
  overlap) and the cursor is put back where the row's button left it. map-editor-m3-auto presses the
  lower half of the dial (`press=244x308`) and expects the placer's angle to follow
  (`expect=placer_angle:270:20`).
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
  **Resolved (05-11):** the rule is a pure function of the vertex sheet the panel already reads, so
  `panels_logic.isValidHeight` ports it (the C++'s own f32 arithmetic) and the ramp paints a refused
  vertex red. Both tiers pin the same sheets (`editor-bridge: M3 height rule ok` with the engine's own
  function, panels_logic `isValidHeight: ...`) and count the refused vertices of coldwinter, 0 of 9409
  on each side (`map-editor-engine: M3 height rule`).
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
  **Resolved (quick task 2026-10-03-script-path-wheel-link-pad, the user's ruling of 2026-10-03: saves sync through
  the cloud to other computers):** a map stores its script path RELATIVE to its own folder - in practice the bare
  name, '/' the only separator it could hold - and a loader expands it beside the map. `CreateRandomMap` stores the
  name of the map it wrote; the bridge's save writes any ABSOLUTE value as its last component (an older generated
  map, or the shipped intro maps' `C:\a7\data\maps\...`), and every other value is left exactly as the map had it
  (the shipped maps' `maps\Name`, so the 66/66 round trip and an unedited save are unchanged); the game's loaders
  (`iMissionInternal`, `GameCreation`, `CommandsHistory`) expand through `NMapScriptPath` (Formats/fmtMapScriptPath.h),
  splitting on either separator. Two generations of one seed into two folders are byte identical
  (`rmg-determinism: byte identical ok across two folders`). WINDOWS.md entry 5 is fixed.
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

## From 05-09

- **Browse takes one map file at a time.** The MFC's Add dialog took several (OFN_ALLOWMULTISELECT); the picker
  inside the Add patches popup does multi-select over the storages' own list, and Browse (for a map outside them)
  hands over the first file the dialog answers, as `PathSlot` does everywhere else.
- **Check! reads every patch map.** A container's Check! loads each patch through the storages (the MFC did
  too), so a 128-patch container takes a few seconds on the main thread with the window waiting. A dirty
  rectangle or a background read is the next step if it bothers anyone; the findings are the same.
- **The Save button of a shipped file asks for a name after the refusal, not before.** The bridge has no "is this
  name the user's own" read, so a shipped container or graph is found out when Save is pressed (the bridge says
  Save As and writes nothing, and the title says "(shipped, read-only)" from then on). A read of the owning layer
  would let the button say so from Open.
- **The composer files are single files.** The MFC composers kept a list file (Editor\Default*.xml) of
  containers/graphs and saved them all together; the portable composers open and save one file each (D-08: the
  lists are folder scans), with the undo of that file only.
- **The user RMG root is below the mod layer.** A mod's own file of the same storage name shadows the user's
  (D-09's wording); in practice a user file never has a shipped name, since Save As refuses one.
- **Nothing in the composers names a script.** The templates' script field (05-10) is where the RELATIVE-script-path
  ruling of 2026-10-03 applies; this plan baked no absolute script path anywhere, and a generated map still names
  its script by the engine's absolute output path (WINDOWS.md entry 5).
- **Windows / CI.** The new data-only step `test-rmg-composer-roundtrip` is not in `.github/workflows/cross-platform.yml`
  yet (05-08's `test-rmg-determinism` is not either): it runs on macOS arm64 here, and wiring both into the five
  engine-C++ targets is a workflow edit for the orchestrator. The composer frames of `map-editor-m3-auto` need a GUI
  and wait for CI or a hand run on win-home, like every M3 scenario before them.

## From 05-10

- **No thumbnails in the Fields Composer's tabs.** The MFC's terrain and objects tabs drew a bitmap of each tile
  and object; the portable tabs list names (and the tile's index), with the weights and widths. A thumbnail needs
  the bridge to render a tile or an object to a texture the way the minimap tools do; the lists and every edit
  work without it.
- **Save All is Save.** The MFC's Save All wrote every field set it held; the portable composers hold one file each
  (D-08, the same as the containers and graphs in 05-09), so Save All and Save are the same button.
- **Appear points are listed in tiles.** The template's unit-creation grid lists an appear point in tiles at 64
  map units to a tile; the record keeps the engine's map units, so nothing is lost on save.
- **The authored game leg cannot redirect the user root on Windows.** `map-editor-game-reads-it-m3` sets a scratch
  `XDG_DATA_HOME` on macOS and Linux (Platform/Paths.cpp honours it there); on Windows the authored files go to
  the profile's user RMG root under `...\user\authored_*` and are rewritten identically on every run. A Windows
  user root override would make the leg hermetic there too.
- **The composer frames of `map-editor-m3-auto` have no Windows run.** The Fields and Templates frames (1214-1400)
  need a GUI and wait for CI or a hand run on win-home, like every M3 scenario before them. The non-GUI Windows
  tiers (core, app tiers, the MSVC compile of the engine tier and the composer tools, the editor app build) ran on
  win-home after the push; the round trip and the determinism step are now in the Windows and macOS engine jobs of
  `cross-platform.yml`.
- **Check! of a template shows the nested files' findings.** A shipped template's graphs and field sets hold
  things the nested rules flag (a link under 8 parts, for one); they are listed with their own severity, prefixed
  with the nested file's name, so a shipped template can open with nested errors. Only the template's own rules
  (the findings without a prefix) are clean on every shipped template, which the engine tier asserts. The nested
  files are fixed in their own composers. A script list that differs from the first graph's is a warning with a
  take-from-graph fix, not an error.
- **The template composer does not offer to create the graph or field set it names.** A name that is not in the
  storages is a Check! finding with a fix that removes it; making a new graph or field set is the other two
  composers' job.

## From 05-11

Every open item above was weighed for full parity. The three the user waived on 2026-10-03 are decisions now (PARITY rows
L4, MM1 and the final note): the Depth Complexity layer stays greyed, the minimap's markers are not filtered by the palette's
filter, the Windows GUI legs run only in CI or by hand. The two scripts-and-wheel rulings are done (f54732c46, 5c50d8f27).

**Fixed here** (real parity gaps, small enough for this plan): the Heights minimap's red for refused heights, the direction
wheel's hot area (both resolved in their entries above) and the Place tool's ghost (PARITY O7: M1 never drew one and the row had
no evidence; the portable ghost is the object's palette picture at half opacity - a recorded difference from the MFC's
engine-sprite ghost).

**Found here, not fixed:**
- **The Properties window shares its ImGui ID with the docked Properties panel.** `panels_m3.drawPropertiesPanel` begins a
  window named "Properties" and so does `panels.drawProperties`; ImGui treats the two as one window, so the M3 fields are
  appended to the docked panel and the window's own close button and View entry act on a window that is not separate. Giving it
  its own ID (`Properties##window`) turns it into a floating window at (left column + 40, top + 100) over the map, which the
  M2 and M3 scenarios' map clicks then hit (the M2 road clicks failed on it), so it was left as it was; a fix means moving the
  scenarios' clicks or the window's first position.

**Left open (listed in the 05-11 checkpoint report, not decided by this plan):**
- **No thumbnails in the Fields Composer's tabs** (05-10). Objects could reuse the palette's pictures, but a tile's picture
  is rendered from the OPEN map's tileset and a composer works with no map open (or another season's), so it needs a bridge call
  that renders a tile of a named tileset.
- **The Settings window's Maps folder does not move a generated map** (05-08). The MFC always generated under the Data root, so
  nothing the MFC did is missed; honouring a custom folder needs the bridge's output root as a parameter.
- **Browse takes one file at a time** where the MFC's Add dialog took several (05-09); the picker over the storages does take many.
- **A party change does not rename the flags** (05-05) and **a shared link ID is reported, not fixed** (05-05): both are the
  preservation rule (never edit what was not asked) against the MFC's silent `ResetPlayersForFlags` and two-record delete.
- **A flag re-owned through Properties keeps its old type in the engine until the map is reopened** (05-04); the saved record is right.
- **A filter made in the Filters Composer and not saved cannot drive the fire ranges** (05-06).
- **Test hygiene:** older engine tests leave scratch files in zig-out/local-test on macOS (05-04); the authored game leg cannot
  redirect the user root on Windows (05-10).
- **WINDOWS.md entry 7** (the composer frames and the Windows run of two data-only steps): the Windows GUI legs are user-waived
  (CI or by hand); the Windows data-only steps run in CI.
