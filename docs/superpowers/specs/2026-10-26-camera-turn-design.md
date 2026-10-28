# The Game camera turns in four steps (Remastered)

## How to try it

You need the Game, the Map Editor and the turned building pictures. From the repository root (Windows PowerShell
shown; one `zig build` at a time):

1. Build and install: `zig build install-game install-map-editor`.
2. Make the turned pictures of every shipped building, once (about 6 minutes in all):
   `zig build building-views-batch-africa`, then `-common`, `-europe`, `-ussr`.
3. Put them into a mod: `zig build camera-turn-mod`. It writes `zig-out\remastered\mods\remastered_views`
   (outside the install's own `mods` folder).
4. Start the Game with that mod:
   `$env:BK_MODS_ROOT = "$PWD\zig-out\remastered\mods"; & zig-out\game\windows\x86_64\debug\Game.exe -mod=remastered_views`
   (`release` instead of `debug` for an optimized build; on Linux
   `BK_MODS_ROOT=$PWD/zig-out/remastered/mods zig-out/game/linux/x86_64/release/Game -mod=remastered_views`).
5. In the main menu: Options > Settings > Gameplay > **Remastered: On**. Start any mission or custom map.
6. Press `.` (period) to turn the view a quarter turn one way and `,` (comma) to turn it back. Four presses come back
   to the 2003 view. Buildings show the side that faces the camera, the ground, units, props, bridges and the minimap
   turn with the view; picking, orders and scrolling follow it. With Remastered off the keys do nothing.
7. The Map Editor turns buildings too: start `zig-out\game\windows\x86_64\debug\MapEditor.exe -mod=remastered_views`
   with the same `BK_MODS_ROOT`, open a map, select a building with the Select tool and press `Q` or `E`: it turns by a
   quarter, shows that side and stands on its turned footprint. Save the map; the Game (Remastered on) draws and plays
   the building turned. The four-sided props (benches, the France fence, the wooden columns) turn the same way. A
   building without turned pictures does not turn (the status line says so).

M006 puts M005's building views ([building views](2026-10-25-building-views-design.md)) on the screen: with the
Remastered option on ([Remastered option](2026-10-25-remastered-option-design.md)) the player turns the Game camera
by quarter turns. Four steps, not eight (decided 2026-10-26, Johannes): a quarter turn maps the diamond grid of
tiles onto itself. With Remastered off nothing changes. Each slice appends what it built and measured.

## What exists (found in S01, 2026-10-26)

- **The camera.** `CCamera` (`Sources/src/Scene/Camera.cpp`) holds an anchor (the ground point under the screen
  centre), distance, pitch and yaw. `SetMissionCameraPlacement` (`GameTT/iMissionInternal.cpp`) places it at pitch
  -120 degrees and yaw 45 for every mission (the Map Editor's `SetSessionCamera` likewise, plus its unused
  `BkEditorSetYaw` offset, D-12). `Update` moves the anchor along the yaw's ground axes (keys and screen edges) and
  snaps it to whole screen steps; `GetPlacement` builds the view matrix from yaw and pitch. The 2003 camera fitted
  in M005 is exactly this one: world x one pixel right and half a pixel down, world y one right and half up
  (`tools/zig/fixtures/bk-views/camera-2003.json`).
- **Fixed-yaw assumptions.** Everything drawn through the view matrix turns with the yaw: sprite positions and
  depths (`CDrawVisitor::AddSingleSprite`, `matTransform`), meshes (vehicles, guns, planes), roads and rivers
  (`CTerrain::DrawVectorObjects`, world space). What does not:
  - the terrain: `CTerrain::MovePatches` (`Scene/TerrainInternal.cpp`) projects only the map's corner and lays every
    patch and tile out from it with the 45 degree offsets `(j - i) * 32, (j + i) * 16` (the `MAKE_COORD` macros of
    `Scene/MeshBuilders.cpp`, shared by tiles, crosses, noise, war fog and markers) as screen-space vertices; a tile's
    texture is a picture of the ground as the 45 degree camera sees it;
  - `AddVertices` culls a tile by the box of its corners taken as top, right, left, bottom (vertices 0, 1, 2, 3);
  - the patch selection for the terrain (`CTerrain::ExtractVisiblePatches`, `TerraDraw.cpp`) and for the scene's
    objects (`CScene::SelectPatches`, `SceneDraw.cpp`): rectangles built from the screen's corners at yaw 45;
  - the sprite depth bias `fZBias2` (`CScene::Draw`): the ground step (-sqrt 2, sqrt 2) that goes up the screen at 45;
  - sprites: a building, fence, bridge, tree or object is a picture from the 45 degree camera; infantry has 8
    facings drawn for it.
  The 03-06 measurement of `BkEditorSetYaw` (terrain clipped away at any other yaw, sprites standing on nothing) is
  these assumptions at work.
- **Buildings.** `CMOBuilding::UpdateModelWithHP` (`Common/MOBuilding.cpp`) builds or changes its sprite, shadow
  and ground picture from `<objects.xml path>\<damage state + 1>` plus the season's `w` and `s`/`g`
  (`IVisObjBuilder::ChangeObject` swaps the animation in place, so the scene keeps the object).
- **The option.** `NRemastered::Enabled()` (`Main/Remastered.h`, commit dd25a28fa): the option var
  `Options.GamePlay.Remastered`, off in multiplayer, a replay's own mode. The option takes `ON`/`OFF`.
- **M005's turns.** `building-views-batch-<region>` writes every variant's turns to
  `zig-out/remastered/Buildings/<region>/summer/<variant>_r<k>/` (582 folders, 26 298 files, 597 MB), the folder
  layout a shipped variant has, side k + 1 being side k turned by (x, y) -> (-y, x). They are not in `Data/` and
  must not be written there.

## Decisions (S01)

- **The shortest path: turn the camera's yaw, fix what assumes 45 degrees.** Camera step k adds -90 k degrees to
  the yaw (`NCameraTurn::YawOffset`): a camera turned by -90 degrees about its anchor sees the world turned by
  +90, which is the turn (x, y) -> (-y, x) that takes side s to side s + 1. Everything drawn through the view matrix
  is then right without change.
- **The step** is the global var `GFX.World.CameraTurn` (0..3, `Main/CameraTurn.h`), session-local like the zoom
  steps: a GFX.* var no save carries, set to 0 when a mission starts (`CInterfaceMission::Init`). With it 0 every
  changed code path takes its 2003 branch, so Remastered off draws as before.
- **The key** is `.` (period), the command `camera_turn` in the `game_mission` section of `Data/Configs/defconf.cfg`
  (no command used it; `,` is left free for a turn the other way if one is wanted). Each press is one more quarter
  turn (step + 1 mod 4) about the point at the screen centre, only while `NRemastered::Enabled()` and the game is
  not paused for control (`MC_CAMERA_TURN`, handled beside the zoom keys).
- **The ground** is laid out with the turned view's steps: `UpdateGroundTurn` projects one tile along the map's
  columns and rows through the view (rounded to whole pixels, as they are at 45) into `groundTurn`, and the patch
  origins and the `MAKE_COORD` macros use them when turned. A tile's texture goes with its corners, so each tile is
  its 45 degree picture laid on the turned diamond (S03 decides whether tiles need turned art). Turned, a tile is
  culled by the box of all four corners, and both patch selections take the square around the anchor that holds
  the screen at every step (`(w / 2 + h) / sqrt 2` world units each way) and leave the rest to the view volume or
  the sprite's screen test. `CScene::Draw` resets the terrain's layout when the step changes; `fZBias2` turns with
  the camera.
- **Buildings** draw `<objects.xml path>_r<k>` at step k where that folder exists in the data (`IsStreamExist` of
  its `1.san`), else their own picture. For now a building's own side counts as side 1 of its variant (S02 adds the
  building's facing). `CWorldClient::Update` tells every map object when the step changed
  (`SMapObject::SetCameraTurn`, a no-op but for `CMOBuilding`), and a building created later reads the step itself.
  A building's drawn turn is saved as chunk 6 only when it is not 0, so saves at the 2003 view are written as
  before.
- **Where the Game finds the turns: a mod**, as `building-views-game-reads-it` does, with each turn at its 2003
  variant's Data path (`data/buildings/<region>/summer/<variant>_r<k>`); the mod storage lies over Data, so no engine
  path changes and nothing in `Data/`. `zig build camera-turn-mod` copies the batch's output into
  `zig-out/remastered/mods/remastered_views` (with its `mod.xml`), outside the install's `mods` folder, which is
  the player's and is packaged.

## Trying it by hand (S01)

1. `zig build building-views-batch-africa`, `-common`, `-europe`, `-ussr` (one at a time; about 6 minutes in all on
   win-home) if `zig-out/remastered/Buildings` is not there yet, then `zig build install-game camera-turn-mod`.
2. Start the Game with the mods folder pointing at the copy and the mod loaded, for example on Windows from the
   repository root in PowerShell:
   `$env:BK_MODS_ROOT = "$PWD\zig-out\remastered\mods"; & zig-out\game\windows\x86_64\debug\Game.exe -mod=remastered_views`
   (`release` instead of `debug` for an optimized install; on Linux `BK_MODS_ROOT=$PWD/zig-out/remastered/mods
   zig-out/game/linux/x86_64/release/Game -mod=remastered_views`). BK_MODS_ROOT makes that folder the only mods
   folder for the run, so the language mods are not offered then.
3. Options > Settings > Gameplay > Remastered: On. Start any mission or custom map and press `.` to turn by a
   quarter; four presses come back to the 2003 view.

## The tier (S01)

`zig build camera-turn-game-reads-it` (`tools/zig/camera_turn_game_reads_it.zig`): `bk-views side` makes the turns
of the 2003 e_ctownhouse01_2 and e_frenchshed01 into a scratch mod at their Data paths; a map written from
`Data/Maps/river3d.xml` holds both by their objects.xml names. The Game runs it three times, the turn key pressed
between shots (`key=.` was added to the BK_AUTO_UI keys):

- **Objects only, Remastered on, four steps.** Each building is scored like building-views-game-reads-it (outline
  IoU and colour against the decomposed sprite, best offset within 4 pixels) where the turn puts it: the screen
  offset (dx, dy) from the centre (511, 383) becomes (2 dy, -dx / 2) per step, predicted from the 2003 camera and
  the side convention, not from the engine. Measured: IoU 0.994 to 1.000, colour 0.00 (1.29 for the town house at
  step 3, near the top band), offset 0,0 at every step; the best other side of the building there 0.173 to 0.257.
  Floors as building-views-game-reads-it: IoU 0.95, colour 2, the own side best.
- **Terrain only, Remastered on, four steps.** The ground in a 400 x 200 region around the centre against the
  step-0 frame turned: mean RGB difference 6.63, 0.91, 6.74 for steps 1 to 3, against 18.2 to 18.4 not turned and
  18.2 to 18.3 turned the wrong way (step 2's other way is itself). The odd steps differ more because every
  tile's 45 degree picture is stretched onto the turned diamond. Ceiling 12 and a margin of 2. Steps k and k + 2
  over the whole screen but the interface (a half turn keeps the screen on the screen): 1.46 and 1.82; before
  `AddVertices` culled by all four corners the tiles along the top and right edges were missing and the same
  measure read 9.55 and 11.74. Ceiling 3.
- **Objects only, Remastered off.** The key changes no pixel (0 changed) and both 2003 sides stay at their places
  (IoU 0.999 and 0.994, colour 0.00, offset 0,0).

## Rough edges seen in S01 (for S02 to S06)

- **Minimap:** its picture does not turn and the view frame drawn on it keeps its unturned shape (seen in the
  shots); a click on it is not checked. (S05)
- **Flora, fences, bridges, objects:** drawn at the right place but with their 2003 pictures, so seen from the wrong
  side. (S03)
- **Ground art:** each tile's 45 degree picture is laid on the turned diamond, stretched 2:1 the other way at odd
  steps; the grass reads well, sharp features (field edges, tracks) will not. Tile height offsets stay vertical
  (right, the pitch is the same). Roads are drawn through the view matrix and should turn; river3d has none, so
  they are not measured. The river is drawn turned (inside the measured region). (S03)
- **Not turned at all:** the AI passability marker (`CreateAIMarker`), the terrain grid (`CreateGrid`), the map's
  border (`DrawBorder`); war fog and the visibility marker use the turned macros but are not measured.
- **Units:** infantry keeps its 8 facings unshifted; vehicles and guns turn as 3D through the view matrix but are
  not measured; unit and building shadows are not measured. (S04)
- **Buildings:** a building's own facing is taken as side 1 of its variant, so a family's 2003 variants of other
  sides (e_ctownhouse01_1/_2/_4) are not used for the turned steps; a variant without a turn made keeps its own
  picture; the frame index of animated building packs and the passenger icons' heights (from the sprite's depth)
  are not re-applied after a swap. (S02)
- **Draw order:** `fZBias2` turns, but overlapping sprites at a turned step are not measured. (S05)
- **Picking, scrolling, saves, replays:** the cursor's ground point and sprite picking go through the turned view
  but are not measured; scrolling follows the turned axes (keys and screen edges move the view the way the screen
  faces) but is not measured; the step is not in a save (every mission starts at step 0), a building saved turned
  remembers its side; a replay does not record the step. (S05)
- **Map Editor:** unchanged (the step var is never set there). (S06)

## Sides by facing plus camera (S02, 2026-10-26)

- **The rule** (`Main/CameraTurn.h`, for the Game now and the Map Editor in S06): a building is drawn with turn
  `NCameraTurn::DrawnTurn(facing, step)` = (own facing + camera step) mod 4 of its objects.xml variant, so a variant
  that shows side s is drawn as side (s - 1 + facing + step) mod 4 + 1. `NCameraTurn::ModelPath(variant, turn)` is
  the folder: the variant at turn 0, else `<variant>_r<turn>` where the data holds its `1.san`, else the variant.
  Every picture the building loads comes from that folder (`<damage state>`, `s`, `g`, the season's `w`), so the rule
  holds for every damage state, winter and shadow alike. `CMOBuilding::GetDrawnTurn` applies it, with the facing
  counted only while `NRemastered::Enabled()`.
- **Own facing of a 2003 map object** (decided): its map `Direction` (0..65535, the AI's convention: 0 faces +y,
  16384 faces -x, the same as a variant's 1.xml `Direction` 0 and 90) to the nearest quarter turn,
  `NCameraTurn::Facing(dir) = ((dir + 0x2000) >> 14) & 3`. One quarter of Direction is one side turn
  (x, y) -> (-y, x). The 2003 game ignored it: `CStaticObjects::AddNewBuilding` takes no direction,
  `CGivenPassabilityStObject::GetDir` is 0, so the client always got 0. Measured: all 4415 buildings and
  fortifications in the 57 shipped maps (`bk-map to-xml Data/Maps`, names matched to objects.xml's
  building/fortification entries) have `Direction="0"`, so every 2003 map draws as before at step 0.
  The AI now keeps the map Direction for buildings (`CBuilding::SetMapDirection`, chunk 41 of its save only when
  not 0) and passes it on in the new-object info's `dir`; `CMOBuilding::SetPlacement` turns it into the facing
  (chunk 7 of its save only when not 0) and keeps its sprites at direction 0, as 2003 had them. The building's
  footprint, entrances, armour sides and `GetDir` do not turn: a building at a facing other than 0 stands on its
  variant's own footprint (S06 decides how the Map Editor's turned buildings get a turned footprint).
- **Option changes.** `CWorldClient::Update` redraws every building when the step or the option changes (the
  option decides whether the facing counts); a building redraws only if its drawn turn changed. With the option
  off the drawn turn is always 0: the 2003 picture, whatever the map's Direction.
- **The family's 2003 sides** (M005's inventory): `bk-views side` now makes turn k of a variant from the family's
  2003 variant that shows that side (the inventory's measured side, eye turn k), in the same colourway
  (`sameColourway`) and holding every picture the variant holds (every damage state, shadow, ground and winter
  picture); otherwise it draws the turn with the blocks as before. The sibling's pictures are packed unmoved, with
  the zero cross moved to the variant's turned zero cross (the sibling's own plus the projection of the family's
  measured shift, rounded to a pixel). So the engine only ever looks for `<variant>_r<k>`, and the Resource
  Editor's Sides window, the Game and the Map Editor all see the 2003 art where it exists. `sources.json` names the
  sibling (`"sibling"`), the batch report lists such turns as `r<k> = <sibling>`. Example: e_ctownhouse01_2's
  turn 2 (side 4) is e_ctownhouse01_4; its turn 3 (side 1) stays drawn, because e_ctownhouse01_1 is another
  colourway (hue distance 0.163). The sibling's zero cross lies 10, 122 pixels from the turned one (shift
  -179.2, 165.3 world units): the 2003 variants of one building do not share a zero cross.
- **A variant without a turn** (no `_r<k>` folder in the data, e.g. a mod's building nobody ran Generate sides on)
  keeps its own picture at that turn (`ModelPath`'s last case). M005's batch makes turns for all 194 variants, so
  in the Remastered data every shipped building has all four.
- **Frame index.** All 2048 `.san` files under `Data/Buildings` are one-sprite packs (counted with
  `bk_views_pack.decodePack`), so a 2003 building has no frames to lose; `ChangeObject` carries the frame index over
  anyway. For a mod's multi-sprite pack, whose turn bk-views packs from sprite 0 (the turned pack has one sprite and
  clamps the index), the building keeps its own frame index (`Create`'s, saved as chunk 8 only while drawn turned)
  and sets it again after every redraw.
- **Passengers' HP bars** stand on the sprite's nearest depth (`CalcPassangerHPAdd`), taken when a unit enters; a
  redraw now places every passenger's bar again from the new picture's depth (`GetPassangerIconPlacement`, shared
  with `Load`) and repositions the icons. Not measured in a capture.

### The tier (S02)

`camera-turn-game-reads-it` gains a second map, `facing.xml`: E_CTownHouse01_2 at own facing 3 (Direction 49152)
and E_FrenchShed01 damaged (HP 0.3, its second damage state) at own facing 2, at the places of `turn.xml`. With
Remastered on each runs alone (turned, the tall town house stands over the shed at some steps) and must show turn
(facing + step) mod 4 of its state where the turn puts it, better than any other turn there; with the option off
both run together and must show their 2003 pictures at offset 0,0 and the key must change no pixel. A damaged state
is measured over its rubble picture (`2g`), which the Game draws with the objects. Before the scores, the town
house's turn 2 is compared with the decomposed 2003 e_ctownhouse01_4: IoU 1.000, colour 0.00.

Measured (win-home, debug install):
- facing 3 town house, steps 0..3 (turns 3, 0, 1, 2): IoU 1.000 / 0.999 / 1.000 / 0.998, colour 0.00..0.18,
  offset 0,0; best other turn 0.200..0.245.
- facing 2 damaged shed, steps 0..3 (turns 2, 3, 0, 1): IoU 0.999 / 0.999 / 0.993 / 0.999, colour 0.00..0.29,
  offset 0,0; best other turn 0.196..0.211.
- off: town house 0.999 / 0.00, damaged shed 0.993 / 0.18, both at 0,0; 0 pixels changed by the key.
- The S01 parts unchanged: objects IoU 0.994..1.000 (turn 2 now the sibling picture, 0.999), terrain 6.63 / 0.91 /
  6.74, half turns 1.48 and 1.82.

Before the shed ran alone and before the rubble was composited, the run failed honestly: the town house over the
shed's rubble at step 1 (IoU 0.858), the "Game saved" line over the town house at the top band (0.939), and
the damaged shed without its rubble (0.82). With the AI's map Direction taken only while Remastered was on at
mission start, the facing never arrived (the tier sets the option after the map loads): the facing is therefore
kept always and counted at draw time.

### Rough edges left (S02)

- A building at a facing other than 0 keeps its variant's own footprint, entrances and armour sides (only its
  picture turns). Shipped maps have none; S06 places them and must decide (turn the passability with the facing,
  or keep facings for square footprints).
- Shadows and winter are covered by the rule (same folder) but not measured in a capture: the objects-only shots
  disable shadows and river3d is a summer map.
- Passenger bar heights after a redraw are not measured.
- The batch output in `zig-out/remastered/Buildings` (and so `camera-turn-mod`) holds siblings only after the
  `building-views-batch-<region>` steps run again.

### Map Editor tiers (S02 step 0)

S01 changed Scene code the Map Editor shares and left `test-editor-bridge` and `test-map-editor-engine` unfinished.
Run on win-home (Windows x64, debug), one at a time, all exit 0: `test-editor-bridge` in its five CI shards
(`-Deditor-bridge-shard=1..5`: 13.9, 22.4, 11.3, 8.2 and 21.9 minutes; shards 1 and 2 on the S01 code, 3 to 5 with
S02's changes), `test-map-editor-engine` (11.0 minutes, with S02), and `map-editor-host-check map-editor-smoke
map-editor-game-reads-it-m3` (19.8 minutes in one call, with S02). No regression was found, so nothing was fixed.
The whole editor-bridge list is longer than one foreground command on this host: run it by shards.

## Ground, roads, rivers, flora and objects turned (S03, 2026-10-27)

### What the rest of the world is made of (found in S03)

- **Terrain tiles** (`CreateTiles`, `Scene/MeshBuilders.cpp`): each tile is four screen-space vertices with the
  tileset's four mapping points, the top, right, left and bottom corners of a 64 x 32 pixel diamond in
  `tileset_c.dds` (256 x 512; `tileset.xml`'s `maps0..3`). A picture holds twice the detail along the screen's x as
  along its y. **Field edges** are crosses: the overlying terrain's tile picture through a crosset mask (another
  diamond, `crosset.xml`), stored per patch in the map (`BaseCrosses`, `LayerCrosses`). The **noise** layer maps a
  square texture by world position (isotropic in the world).
- **Roads and rivers** (`CTerrain::DrawVectorObjects`): world-space meshes through the view matrix. The **border**
  (`DrawBorder`) and the **grid** (`CreateGrid`) are world-space too. The **war fog** (`CreateWarFog`) and the
  visibility marker are screen-space meshes on the turned `MAKE_COORD` layout (S01); the **AI passability marker**
  (`CreateAIMarker`) had its own 45 degree layout.
- **Objects** (`CMOObject`, `Common/MOObject.cpp`): `Data/Objects` holds sprite folders (`1`, `1s` shadow,
  `1w`/`1ws` winter): Flora 65, SimpleObjects 248 (game type `object`), TerraObjects 272 (`terraobj`, drawn with the
  terrain's depth, flat decals: dirt, garbage, ravines, dunes, mounts, coast lines, shell holes). Footprints
  (`passability` of 1.xml): SimpleObjects 141 square, 97 a longer rectangle, 10 none; Flora 60 square, 5 rectangles.
- **Fences** (`Data/Fences`, 16 kinds): one sprite-animation pack (`1.san`, not a sprite pack bk-views reads) whose
  frames are the segments; 1.xml lists four directions, North-east, North-west, South-west and South-east (the
  Resource Editor's names), each with centre, left-, right- and centre-damaged segments. World +y is the screen's
  north-east, -x its north-west, so the four directions are quarter turns (x, y) -> (-y, x), M005's side
  convention. A map stores a fence's direction packed in its FrameIndex (0x10000 + the direction's bit).
- **Bridges** (`Data/Bridges`, 10 kinds, `01` along x and `02` along y): per span three sprites (back girder, slab,
  front girder) from begin, line and end segment lists, intact, damaged and destroyed.
- **Shipped maps** (all 57, `bk-map to-xml`): every static object has map Direction 0 (bridges 200, fences 34 724,
  flora 52 176, simple objects 6 373, terra objects 27 952, mines 803, flags 79), as the buildings (S02).

### Decisions per kind (S03)

The shortest path per kind, no new pictures; the rule is S02's `NCameraTurn::DrawnTurn(facing, step)` with a static
object's own facing 0 (the AI drops its map Direction, as for 2003 buildings; all shipped maps have 0) and a fence's
own facing its direction.

- **Terrain tiles: turn the mapping, not the art.** At an odd step each tile (and each cross's tile picture) is laid
  with its picture's top on the tile's top corner (`groundTurn.nMaps`, `TILE_MAP`), so the picture keeps the
  detail it was drawn with; going with the ground it was stretched 2:1 the other way (S01's rough edge). The
  crosset's masks (the shapes of field edges) and the noise still go with the ground, so fields, roads and their
  edges turn exactly. At a half turn the 2003 mapping is kept (a half turn of the ground is a half turn of the
  screen, pixel for pixel). Turned art for tiles was not made: a tile picture is grain, and the random choice among
  a terrain type's tiles (and their flipped variants) already puts any two side by side.
- **Roads, rivers, border, grid:** no change; they are world space and turn with the view (measured below; the grid
  is a key-toggled overlay no scene switch shows, not captured).
- **War fog (fixed):** `CWorldBase::GetVisibilityRectBounds` built the rectangle (in x + y and x - y) the AI returns
  visibilities for from the screen's left top and right bottom ground points as at 45 degrees; turned, those are
  other corners and the rectangle missed the screen, so the turned view was all fog (dessau: block difference 16
  against 2.0 now). Turned, it bounds all four screen corners (with the 2003 margin of 3 cells), and the fog is
  asked again when the step changes. The AI marker uses the same rectangle and is laid out with the turned tile
  steps (`AI_MARK_X/Y`); a debug overlay, not captured.
- **Flora: its own picture at every step** (M005's 2003 sprite fallback for thin objects: a crown looks alike from
  every side; its shadow stays as drawn, like M005's light that turns with the camera). A tree on a rectangular
  footprint (5 of 65) is mirrored at odd steps, as the props.
- **SimpleObjects: own picture, mirrored at odd steps on a longer rectangle.** On a square footprint or none (151)
  the own picture, as above. On a longer rectangle (97: benches, boxes, counters, piers, sandbag and fence pieces)
  the picture mirrored about the zero cross at an odd step, which lays it on the turned footprint (world (x, y) to
  (-y, -x), M005's mirrored 2003 sprite), its shadow kept as M005 keeps a mirrored sprite's. Done in the sprite
  pass (`SBasicSpriteInfo::bMirror`), so no data. Not relit (M005 relit its mirrors by the blocks' faces).
- **TerraObjects: the picture turned as the ground turns** (`SBasicSpriteInfo::nFlatTurn`): every square of the
  sprite and its shadow is laid on the screen by the quarter turn of the ground about the zero cross,
  (dx, dy) -> (2 dy, -dx / 2), exactly what the step-0 frame turned shows of a flat decal (a coast line stays on its
  river).
- **Fences: the existing directional variants.** A segment draws the segment at the same place in the same list
  (centre, left, right or centre damaged) of direction `DrawnTurn(direction, step)` (`TurnedFenceFrame`); kept
  across damage (the AI's frame changes) and saves (chunk 6 only while turned).
- **Bridges: left for later.** A quarter turn of a bridge along x would be the kind's `02` variant along y, but the
  two hold different span lists (woodenlittle: spans of 5, 2 and 7 cells against 7, 2 and 7), so the segments do
  not map one to one, and each span is three sprites whose back and front girders trade places at a half turn. A
  bridge now draws its 2003 pictures at their turned places. It needs a segment map by span kind and length, or
  turned art from M005's pipeline.
- **The mirror and the half turn in the sprite pass** keep the 2003 pixels: along an axis the turn reverses, point
  sampling picked the texel one further for every pixel (a foreign column at one edge of each square, one dropped
  at the other: measured on the bench). The mapping goes back one texel there; at an odd step, where a square is
  stretched 2:1, it is pulled half a texel inside the square. Measured: the mirrored bench is the 2003 picture
  mirrored with 0 pixels different, the half-turned ravine the 2003 picture turned with 0 of 4 678.

### The tier (S03)

`zig build camera-turn-world-game-reads-it` (`tools/zig/camera_turn_world_game_reads_it.zig`, about 6.5 minutes on
win-home): the real Game, Remastered on, the turn key between four shots, each measured against the step-0 frame
turned:

- **Ground** (objects, units, shadows, effects off): the shipped `dessau.bzm` at a spot with a road, field edges,
  the river with a bridge, trees, fences and objects (found by scanning the 57 maps), and `road3d.xml`, whose whole
  map and border are on the screen. Mean RGB difference of 9 x 9 pixel blocks (the shapes of the ground without
  the grain of its pictures) against the step-0 frame turned, not turned, and turned the other way; the camera turns
  about its anchor at height 0, so raised ground moves by its height on a turn and the measure finds that lift
  (here 0 to 2 pixels). Ceiling 6, margin 2; steps k and k + 2 over the whole screen, ceiling 6. The grain:
  ln of the ratio of horizontal to vertical brightness steps against step 0, at most 0.05 on road3d's grass.
- **Scene:** dessau with everything drawn: turned must be closer than not turned and the other way, ceiling 24 (no
  margin: its buildings have no turned sides in tracked data and its trees are their own pictures).
- **Objects** alone on a ring around the centre of a map written from road3d.xml (a quarter turn takes the ring onto
  itself): `_Lime` and `ShellBox01` (own picture), `Bench01` (1 x 2: mirrored at odd steps), `DirtRavine01` (flat
  turn), four `VillageFence` segments of the four directions (one segment of each kind per direction). Each is cut
  from the step-0 frame around its zero cross and scored where the turn puts it: outline IoU and colour at the best
  offset within 4 pixels, against the wrong picture (not mirrored, not turned, its own direction). Floors IoU 0.9
  (0.8 flat), colour 8 (16 flat; a soft edge blends with the backdrop, whose shade changes with height).
- **Remastered off:** the key changes no pixel of any object.

Measured (win-home, Windows x64, debug install):

| Ground | step 1 | step 2 | step 3 | half turns 0-2 / 1-3 |
|---|---|---|---|---|
| dessau turned / not / other way | 2.00 / 23.53 / 25.23 | 2.23 / 24.74 / 2.23 | 2.49 / 23.24 / 23.29 | 1.52 / 3.17 |
| road3d turned / not / other way | 1.47 / 22.55 / 28.43 | 0.20 / 27.86 / 0.20 | 1.43 / 22.72 / 25.38 | 0.18 / 1.54 |
| grain ln (dessau reported, road3d gated) | -0.075 / -0.006 | 0.015 / -0.001 | -0.090 / -0.004 | |
| scene turned / not / other way | 20.38 / 23.31 / 33.61 | 19.40 / 36.99 / 19.40 | 17.93 / 28.13 / 34.81 | |

- Before the war fog fix, dessau's turned difference was 16.1 to 16.4 (all fog). The grain of S01's river3d shots
  (tiles going with the ground) was ln -0.149 at odd steps; S03's river3d shots -0.005 and -0.004.
- Objects: the four fence segments IoU 1.000, colour 0.00, offset 0,0 at every step, their own direction's picture
  0.061 to 0.065 at odd steps; the bench 1.000 (mirrored at odd steps; not mirrored 0.251); the box 1.000; the tree
  1.000 (0.933 at step 1, where its crown reaches the message line, which is left out); the ravine 0.895 / 1.000 /
  0.894 (colour 9.2, 0.0, 9.3; not turned 0.033 to 0.040).
- Off: 0 pixels changed by the key.

`camera-turn-game-reads-it` measures its ground the same way now (a tile's grain no longer turns with the ground at
an odd step, so S01's per-pixel ground fell to a margin of 2.02): blocks 2.91 / 0.13 / 2.83 against 9.67 to 11.40,
half turns 0.25 and 2.84 (ceiling 4); its buildings unchanged (IoU 0.994 to 1.000).

### Rough edges left (S03)

- **Bridges** draw their 2003 pictures at their turned places (above).
- **Light:** mirrored props are not relit (the lit side flips at odd steps); own-picture trees and props keep the
  2003 lit side and shadow, as M005's camera-fixed light; the ground's vertex shade follows the world sun, which
  only shows on hills. A terra object's painted relief (ravines, dunes, mounts, coasts) turns with it, lit from the
  turned side, and is stretched 2:1 at an odd step like the ground it lies on.
- **Tiles at an odd step** show their grain upright, so the grain does not continue across tiles as the ground
  turns (as at step 0, where neighbouring tiles are random); a feature painted inside one tile picture would turn
  in place (the tilesets were not searched for one).
- **2003 sibling variants of props** (box, bench, columns, francefence 01..04 show four sides) are not used yet: the
  mirror stands in at odd steps and the own picture at the half turn. Turned art for props (bk-views side reads
  only building 1.xml and sprite packs) is left for later.
- **Picking** of mirrored and flat-turned sprites still uses the 2003 rectangle (S05).
- **The turn pivot** is the camera's anchor at height 0: on raised ground the point under the screen centre moves by
  its height on a turn (1 to 2 pixels at the dessau spot; more on hills). S05 may pivot about the ground point.
- **A static object's map Direction** is not kept by the AI (only buildings', S02); every shipped map has 0. S06
  needs it if the Map Editor turns objects.
- The tier's dessau scene check has no margin (no turned buildings in tracked data).

### Other tiers (S03)

Run on win-home (Windows x64, debug), one at a time, after the last engine change: `camera-turn-world-game-reads-it`
and `camera-turn-game-reads-it` pass (about 6.5 minutes each); `building-views-game-reads-it` passes (IoU 0.994 to
1.000, colour 0.00); `remastered-game-reads-it`: the tier's own tests pass, its 4 failures are the `l10n_measure`
hb-view tests (hb-view is not installed on Windows; as in S01 and S02). The Scene and Common code the Map Editor
shares changed (its step stays 0, so it takes the 2003 paths): `test-map-editor-engine map-editor-smoke` in one call,
exit 0 (5.7 minutes). The editor bridge's own code is untouched, so `test-editor-bridge` was not run. Step 0: the four
`building-views-batch-<region>` steps (44 s, 55 s, 2 min 2 s, 3 min 18 s) and `building-views-batch-report` ran again
on S02's bk-views, so `zig-out/remastered/Buildings` and `batch.md` hold the 2003 sibling sides (43 variants take one
or more turns from a sibling); `camera-turn-mod` copies them when it runs next.

## Units at every step, bridges and four-sided props (S04, 2026-10-27)

### What units are made of (found in S04)

- **Infantry** (`Data/Units/Humans`): a sprite animation (`CSpriteAnimation`, `Formats/fmtAnimation.h`) of eight
  directions per animation; `GetRect` picks the direction from the unit's world direction (0..65535, the AI's
  convention: 0 faces +y, 16384 faces -x). The pictures were drawn for the 45 degree camera.
- **Vehicles and guns** (`.mod` meshes, `CMeshVisObj`): drawn through the view matrix, so they turn with it; lit by
  the scene's sun (`CScene::sunlight`, world space) and shadowed by projecting the mesh along the sun
  (`DrawShadow`). A gun's crew are infantry sprites.
- **Effects** (`CEffectVisObj`): sprite sub-effects are `CSpriteVisObj`s set to the effect's direction (facing
  pictures as infantry's where a sprite has directions); particles and smoke are world-space billboards; tank tracks
  (`SMechTrace`) and gun traces are world-space quads. All of these turn with the view.
- **Icons** (HP bars, status icons, passengers' bars, `Scene/Icon*.cpp`): world points at the object's position plus
  an offset laid out along the 45 degree view's axes (`RepositionIconsLocal`: a row goes along world (1, 1), which
  is screen right only at 45 degrees; a passenger's bar stands out towards the viewer by (sqrt 2, -sqrt 2) times
  its depth, `CalcPassangerHPAdd`). Selection only changes the icons' alpha.
- Two more 45 degree offsets: the ground point a falling plane is followed by (`MOUnitMechanical`, harness only) and
  a raised sound's 2D point (`CSoundScene::To2DSoundPos`), both "up the screen by the height".

### Decisions (S04)

- **Infantry and directional effects: the facing a quarter turn on.** At step k a unit facing d is seen as the 2003
  camera sees one facing d + 16384 k (the view shows the world turned by k quarters), so `CSpriteVisObj` asks its
  animation for `NCameraTurn::ViewDirection(d, k)`: two of infantry's eight pictures per step. The sprite keeps the
  step it drew (`nViewTurn`, not saved) and takes its picture again when the step changes, also while the game is
  paused. The unit's own direction (AI, saves, picking) does not change.
- **Vehicles and guns: the sun turns with the camera.** Meshes already turn with the view; the sun is turned by the
  camera's yaw offset for the frame (`CScene::Draw`, a local `sun` for mesh light and mesh shadows), so a tank is
  lit and casts its shadow to the same side of the screen at every step, as the 2003 pictures of infantry, trees
  and props and M005's turned building sides are (camera-fixed light). The ground's vertex shade still follows the
  world sun (S03's rough edge). *Reversed in round 2: the sun stays in the world ("The sun fixed in the world",
  below).*
- **Icons, the plane camera and sound: `NCameraTurn::ViewOffset`.** An offset laid out along the 45 degree view's
  axes is turned by the camera's yaw offset, so an icon row stays a row on the screen and a passenger's bar stays in
  front of its building; each icon applies it when it is placed beside its object every frame (`Reposition`).
- **Bridges: the 2003 span that shows the span turned** (`CMOBridgeSpan::ApplyCameraTurn`). At an odd step the
  kind's variant along the other axis (`_01` runs along x, `_02` along y), at the half turn its own; of that
  variant's spans of the same damage state, the one whose slab footprint best covers the span's own footprint
  turned by the step (rectangle IoU above 0.5). A line span turns onto the other variant's line (footprint and
  zero cross line up exactly in the wooden and asphalt kinds; the railway bridge's 8 x 5 line against 6 x 8 is a
  tile narrower); a begin or end span onto the other variant's begin or end (or, at
  the half turn, its own other end), which in the shipped kinds is up to a tile longer or narrower (woodenlittle
  `_01`'s begin is 5 tiles long, `_02`'s 7). The span's three parts (back girder, slab, front girder) take that
  span's segments and stand at its parts' places turned back by the step (`ViewOffset` of their `RelPos`), so the
  girder that is far now is the back one and is drawn behind the slab: the half turn trades the girders by place.
  A part is placed before its picture is taken: a sprite pack keeps the position of its last `Update` and the scene
  does not update static objects (placed after, the girders stood a turn behind and the line spans scored 0.86 to
  0.89). Where the target variant has no shadow pack the part's shadow is hidden. A span with no match (a kind
  without a `_02`, a mod's bridge) keeps its 2003 pictures at its turned place. A bridge object's map FrameIndex is
  its span type (1 begin, 2 line, 4 end). Saved (chunks 3 to 6 of the span) only while turned. No turned art was
  made: every shipped span finds a 2003 picture.
- **Props of four sides: their 2003 variants** (`IsFourSided`, `CMOObject::ApplyCameraTurn`). The bench
  (`ussr\summer\bench`), the France fence (`europe\summer\e_francefence`) and the wooden columns
  (`bigcolumnwithleg`, `biglightcolumn`, `littlecolumnwithleg`, `littlelightcolumn`) hold variants `01` to `04`,
  each the one before turned by (x, y) -> (-y, x), the side convention (read from the pictures: the bench's seat
  faces +x, +y, -x, -y; the columns' leg and lamp turn the same way; the France fence's wall is taken to follow the
  same naming). Such a prop draws variant (own - 1 + step) mod 4 + 1, in its season, with its shadow, instead of the
  mirror. The box's `01` to `04` are four different things (lines and zigzags), not sides: left on the mirror.
  Bench `02` is brown where `01`, `03` and `04` are green; it is drawn anyway (the 2003 side, another colourway).
- **Relighting mirrored props: not done.** M005 relit a mirrored sprite by per-class gains from the blocks' shading
  classes (bk-views); the engine has no classes for a prop's pixels, so it is not short. Mirrored props keep their
  lit side flipped at odd steps (S03's rough edge).

### The tiers (S04)

`zig build camera-turn-units-game-reads-it` (`tools/zig/camera_turn_units_game_reads_it.zig`, about 4 minutes on
win-home): a map written from road3d.xml with units of player 0 on rings a quarter turn takes onto themselves, each
facing along its ring (map Direction = its eighth times 8192): German snipers (one soldier, half health so the bar
shows) at seven of the eight eighths of an inner ring, T-34s at the even and 5 cm Pak 38s at the odd eighths of an
outer ring. A quarter turn takes the unit at eighth e to eighth e + 2 and shows it as the 2003 camera shows a unit a
quarter turn on, which is the unit standing there at step 0: each unit is scored against the step-0 frame at the
place it moves to. The game is paused (SPACE, `runGameWith`) before the shots: unpaused, idling soldiers scored IoU
0.5 to 0.73 against any facing. Measured (win-home, Windows x64, debug install):

- snipers: IoU 1.000, colour 0.00 at every step; their facing not shifted 0.518 to 0.668; the inner ring's gap
  moved with the turn (0 pixels drawn where it went);
- T-34s: IoU 0.999 to 1.000, colour 0.01 to 0.15 (lit as the step-0 tank of that facing: the sun turns);
- Pak 38s: IoU 0.875 to 0.996, colour 1.2 to 14.1 over three runs (the gun and its ammunition box match; its
  crew stand in animations whose phases differ from gun to gun); floors 0.85 and 16;
- tank shadows (the frame with shadows darker than one without, terrain on): IoU 1.000 against the step-0 shadow
  at the turned place, 566 to 743 pixels; guns' shadows 0.50 to 0.99, reported only (the crews' poses differ
  between the two runs);
- Remastered off: the key changed 0 unit pixels.

`camera-turn-world-game-reads-it` gains one run (now about 7.5 minutes of Game runs plus its compile): benches and
wooden columns with legs, variants 01 to 04 two eighths apart on the ring, and line spans of the wooden little
bridge along x and y alternating on an inner ring with eighth 6 empty. Measured: every bench and column IoU 1.000,
colour 0.00 at every step, its own picture 0.020 to 0.447; bridge spans 1.000 (`_02` turned onto `_01`'s line),
0.958 (`_01` onto `_02`'s, colour 2.7) and 0.950 at the half turn (colour 2.9: the girders' places round to other
pixels), against 0.49 for the 2003 picture at the turned place (S03's behaviour); the gap moved with the turn. The
mirrored prop is now Swing01 (1.000, not mirrored 0.145). The dessau scene's blocks went from 20.4 / 19.4 / 17.9 to
15.5 / 19.8 / 11.7 (its bridge and its units now turn). Remastered off, fences, props and bridge spans: 0 pixels
changed.

### Rough edges left (S04)

- Bridge begin and end spans are measured only through the dessau scene: a lone span's turned picture is another
  variant's or the other end's, a tile longer or narrower in the shipped kinds, of which a ring of lone spans holds
  no step-0 picture.
- The guns' crews and their shadows are not held to their pixels (animation phases); passengers' bars, unit status
  icon rows, directional effect sprites, tracks and gun traces turn by construction but are not captured.
- Mirrored props are not relit; the France fence's side order is assumed from the other families'.
- A tier's Game run that asserts waits for its 240 s timeout (an assert dialog), which can push a tier past 10
  minutes; it then fails, as it should.

## Draw order, picking, scrolling, the minimap, saves and replays (S05, 2026-10-27)

### What was found (S05)

- **Draw order** needed no change: a sprite's depth comes from the turned view matrix (`relpos.z`) and S01's turned
  `fZBias2`, and the z-buffer sorts the rest. Measured below.
- **Scrolling** needed no change: the arrow keys and the screen's edges move the anchor along the turned yaw's axes
  (S01), and the trackpad pan goes through `GetPos3`, which projects through the turned view. Measured below.
- **Picking a sprite** (`CSpriteVisObj::IsHit`) tested the 2003 picture's rectangle at the zero cross, so a mirrored
  prop (S03) was picked by its unmirrored outline. A flat-turned terra object has the same test, but the game never
  picks terra objects (`CWorldClient::UpdatePick` stops at fences).
- **A frame selection** (`CScene::Pick` by rectangle) chose the cells to look in with `SelectPatches2`, whose
  rectangle is built from the 45 degree view's screen axes: turned, it looked in the wrong cells.
- **The minimap** drew the 2003 picture with the view frame of the turned view on it (the screen's corners on the
  ground, a turned shape); its frame clipping (`GetVerticalClippedScreenEdge` and the horizontal one) assumes which map
  edge each screen edge can cross at 45 degrees.
- **Ground under the screen's centre.** `CScene::GetPos3` asks the AI for the line of sight's meeting with the
  terrain, through `IGFX::GetViewVolumeCrosses`, which the GPU backend leaves empty (`GraphicsEngineGpu.cpp`); so
  every screen point is taken on the plane at height 0 (orders, the minimap frame), at every step including step 0.
  Not changed here (the 2003 behaviour on hills needs that stub filled; outside this slice).
- **Saves.** The step is a `GFX.*` var, which no save carries. A load reads the mission interface back and then
  runs its `Init`, which puts every mission at step 0.
- **Replays.** `-sh<file>` records the commands, `-lh<file>` plays them (a file name ending in `.xml` is taken for
  the map). Every `-lh` play stopped with a UBSan panic before its first segment: the XML reader takes each field's
  present value first (`CTreeAccessor::AddIntData`), and `CGroupCommand::bPlaceInQueue` was indeterminate (fixed,
  with `CShowAreasCommand::bShow`). The history holds commands only; nothing of the view.
- **Multiplayer.** `NRemastered::Enabled()` is false in multiplayer (gameplay rules must match on every machine),
  so the turn key did nothing there. `playersCameraAnchors` are world units (a tile is 32 sqrt 2), and the shipped
  multiplayer maps put each player's anchor near the player's own corner or edge.
- **The controls screen** lists no keys (no text names the zoom or turn keys), so only `defconf.cfg` changes.

### Decisions (S05)

- **Turning back: `,`**, the command `camera_turn_back` in `game_mission` (`MC_CAMERA_TURN_BACK`), a quarter turn
  the other way (step - 1 mod 4). BK_AUTO_UI names it `COMMA` (a comma separates the schedule's entries).
- **Who may turn: the player's own Remastered option** (`NCameraTurn::Allowed`, the option var itself), also in
  multiplayer and in a replay of either mode: the step is local view state. Gameplay rules stay behind
  `NRemastered::Enabled()`.
- **The pivot is the ground under the screen's centre** (`NCameraTurn::GroundUnderCentre`): walking the centre's line
  of sight down from the eye against `ITerrain::GetHeight` (the heights units stand on), the first ground it meets.
  `CInterfaceMission::TurnCamera` turns the anchor's offset from that point with the view, so the point stays under
  the centre; on flat ground it is the anchor, as before. It does not use `GetPos3` (above). Where a hill hides the
  point from the turned eye, the point under the centre is the hill: a turn shows what is in front.
- **Picking what is drawn:** `SBasicSpriteInfo::UnturnOffset` takes a screen offset from the zero cross back into the
  2003 picture (the flat turn undone, then the mirror), for the point pick and the rectangle pick of a sprite. A
  frame selection turned looks in every cell of the box around its rectangle's ground corners (as wide as
  `SelectPatches2`'s), and `IsHit` decides.
- **The minimap turns with the view** (chosen over a fixed picture with a turned frame: the minimap then shows the
  map as the view does, the frame keeps the screen's upright shape, and a player's own corner is at the bottom of
  both). `CUIMiniMap::PointToTextureMiniMap` turns every map point about the map's middle by the step before the
  2003 layout (`ProjectMiniMap`), and `TextureMiniMapToPoint` turns a click back, so the picture, the war fog, the
  units, the markers, fire ranges, circles, the frame and the clicks all follow one rule. A turned map keeps the
  diamond's bounding box (its width is x + y cells either way), so it fits the frame. Radii of circles are measured on
  the 2003 layout (a circle is the same turned); a plane's altitude still lifts it up the picture; turned, the frame's
  edges are clipped to the map as segments (`GetClippedEdge`, Liang-Barsky); the resampled picture is made again when
  the step changes (`Draw`).
- **Saves:** the mission interface writes the step as chunk 15 only when it is not 0, so a save at the 2003 view is
  written as before; a load keeps the chunk's step (0 without it) and sets it on `CMD_LOAD_FINISHED`, once (the
  settings screen sends that command too and must keep the step).
- **The multiplayer start step** (`NCameraTurn::StartStep`): with the option on, a multiplayer game starts each
  player's view at the step that puts the player's corner, its camera anchor's offset from the map's middle, at the
  bottom of the screen. Screen down is world (1, -1) at step 0 and step k shows an offset where step 0 shows it turned
  k times by (x, y) -> (-y, x); the step that points the corner most nearly down wins, step 0 on a tie (no anchor,
  or one on the middle). A replay of a multiplayer game loads as one (`MultiplayerGame` from the history), so it
  starts at the step of the player it shows; turns made while playing are not recorded (view state). A single player
  mission starts at step 0, as in S01 (its anchor is where the designer put the opening view). `BK_MAP_TRACE` prints
  the rule's step for every mission (`start turn <k> (applied | single player: 0)`).
- **The step stays out of the game state:** nothing the AI runs reads it (the AI's war fog rectangle and the
  visibility queries the client makes read, S03). The proof is a replay played at two steps (below).
  `BK_AI_STATE_TRACE` prints the multiplayer checksum's terms of every unit (centre, direction, HP, morale) as a CRC
  once a second of game time.
- **Harness:** BK_AUTO_UI gains `pick=<x>x<y>[x<step>x<n>]` (`TracePick`: what the cursor picks there, or on a grid),
  `drag=<x1>x<y1>x<x2>x<y2>` (a frame selection), `anchor` (the anchor, the ground under the centre and the step),
  `key=<name>*<frames>` (a key held) and `COMMA`; `TraceSelection` lists each selected unit or building by name and
  place (`BK_SELECTED:`).

### The tiers (S05)

`zig build camera-turn-control-game-reads-it` (`tools/zig/camera_turn_control_game_reads_it.zig`, about 5 minutes on
win-home):

- **control** (road3d, objects only; snipers of player 0 at eighths 0, 2, 4 of a ring, Swing01, E_House01_1): at every
  step, every point of a grid well inside each drawn picture must pick it; a click where sniper 0 is drawn must select
  it alone; a frame drawn around where snipers 0 and 2 are drawn must select exactly those; the arrow key, the right
  screen edge and a trackpad swipe must move the view along the screen's right as the turn lays it on the ground; at
  step 3 sniper 0 is ordered to a point clicked where the turn draws it.
- **minimap** (the shipped coldwinter, large enough that the frame lies inside the map): with the view on the map's
  middle the frame must keep its step-0 shape, the picture must be the step-0 picture turned about the minimap's
  middle, and a minimap click must bring the frame to the clicked point.
- **hill** (road3d with a hill of height 120): the ground under the centre through three turns and one back.
- **draw order**: four clusters (a shed, four snipers in front of and behind it, four trees); the middle one at steps 0
  to 3 against the others, the middle one turned 1, 2, 3 times (places, unit facings, the shed's own facing) at step
  0, paused, the pause line left out.

Measured (win-home, Windows x64, debug install):

| | step 0 | step 1 | step 2 | step 3 |
|---|---|---|---|---|
| picks inside Swing01 / E_House01_1 / sniper (recall) | 17/17, 339/341, 15/15 | 17/17, 341/342, 21/21 | 17/17, 339/341, 19/19 | 17/17, 246/248, 18/18 |
| of the swing's, off its 2003 outline | 0 | 9 | 0 | 9 |
| click / frame selection | sniper 0 / 0 and 2 | same | same | same |
| key, edge, swipe: cos to the screen's right | 1.000, 1.000, 1.000 | 1.000, 1.000, 0.999 | 1.000 each | 1.000, 1.000, 0.999 |
| minimap frame IoU on the middle; picture turned / not | 1.000 | 1.000; 12.4 / 29.3 | 1.000; 14.4 / 40.8 | 1.000; 12.8 / 30.5 |
| minimap click -> frame's middle | 173,686 -> 172,685 | 149,658 -> 148,657 | 93,670 -> 92,669 | 117,698 -> 116,697 |
| hill: ground under the centre, from step 0's | 0.0 | 1.2 | 1.0 | 1.7 (2.0 back at 2) |
| draw order: pixels differing / drawn (against step 0's shot) | | 0 / 28 667 (0.52) | 0 / 22 681 (0.47) | 0 / 27 861 (0.52) |

The order at step 3, clicked at 776,441: the sniper walked to 1.7 world units from the clicked ground. The house's
two or three missed points of three hundred lie on its picture's ragged edge (a complex sprite is picked by its
squares). On the hill (37.7 high under the centre) the 2003 pivot would move the point 92 world units a turn. The house
at step 3 stands partly under the message line, where no point is counted. A minimap click lands 140 to 200 world
units from step 0's click at the other steps: the click is a whole pixel and the minimap's middle is known to a
pixel, and the turn doubles an error along x; the frame lands on the clicked pixel at every step.

`zig build camera-turn-state-game-reads-it` (`tools/zig/camera_turn_state_game_reads_it.zig`, about 7 minutes with a
Game rebuild):

- **save and load:** a mission saved at step 2 and again at step 0; the first loads at step 2 at the saved anchor with
  0.0046 of the view's pixels differing from the shot before the save; turned once more (step 3), the second loads at
  step 0.
- **replay:** recorded at step 1 (the sniper clicked and ordered where the turn draws them; it walked to 2.2 world
  units from the clicked ground) under a T-34's fire; played at step 0 and turned back and forth with the view moved
  away: 17 of 17 seconds the same units' state (17 distinct states, units 12 down to 7), while the recording run itself
  (another random seed) matched for 1 second only.
- **multiplayer start step:** the player's anchor in each corner of road3d: (+, -) step 0, (-, -) 1, (-, +) 2, (+, +) 3,
  each as expected; every single player mission started at step 0.

### Other tiers (S05)

Run on win-home (Windows x64, debug), one at a time, after the last engine change: `camera-turn-game-reads-it` (buildings
IoU 0.993 to 1.000, terrain blocks 2.91 / 0.13 / 2.84), `camera-turn-world-game-reads-it` (dessau ground 1.99 / 2.23 /
2.44, lifts 1, 1, 2; scene 15.5 / 19.8 / 11.6), `camera-turn-units-game-reads-it` and `building-views-game-reads-it` pass;
`remastered-game-reads-it`, `mod-compat-game-reads-it` and `mod-language-game-reads-it`: 13 of 17, the 4 failures the
known hb-view `l10n_measure` tests (`mod-language-game-reads-it` takes about 18 minutes with its language mods, so it was
run in the background and waited for). Scene and UI code the Map Editor shares changed (its step stays 0):
`test-map-editor-engine map-editor-smoke` in one call, exit 0 (6.5 minutes). `camera-turn-control-game-reads-it` ran again
after the load fix with the same numbers. The editor bridge's own code is untouched, so `test-editor-bridge` was not run.

### Rough edges left (S05)

- **The multiplayer start step is applied by code only:** no tier runs a multiplayer game (no harness for two Games);
  the rule is measured through `BK_MAP_TRACE` on single player maps, and a multiplayer replay's view of each player is
  not run either. That the step never reaches the network rests on the replay proof (the step is in no command, and
  the multiplayer stream carries commands and checksums only).
- **Ground on hills** is the plane at height 0 for every screen point (`GetViewVolumeCrosses` is empty on the GPU
  backend): orders and the minimap frame on raised ground land off the clicked ground at every step, step 0 included.
- **Draw order** is measured with infantry, trees and a building; bridges are not in the clusters (their spans are other
  variants' pictures, S04).
- The minimap's resampled picture is made again on each turn (not timed); a turned frame is clipped by the new segment
  clipping, the 2003 one still at step 0.
- A replay records no turns made while it was recorded; a single player replay starts at step 0.

## The Map Editor turns buildings by the same rule (S06, 2026-10-27)

### What was found (S06)

- **The Map Editor draws with the Game's own objects.** Its bridge runs the AI (`CAIEditor`) and a `CWorldBase`, so a
  placed building is a `CMOBuilding` and a prop a `CMOObject`, drawn by the same code as in the Game. Its camera step
  stays 0. Before S06 its Q and E turned a selected object by a sixteenth (`rotate_step`); on a building or a prop the
  turn was refused ("the engine would not take that placement"), because the AI's `CGivenPassabilityStObject::GetDir`
  is 0 whatever it is told.
- **The AI's stats belong to the database's module.** The stats are polymorphic and their virtual functions live in
  `Main` (`RPGStats.cpp`), so another module cannot copy them (the copy's vtable is missing at link time).
- **Static placements** reach the client through `AIUpdatePlacement`, not `SetPlacement`, and carried `GetDir()`.
- **Every click in the Game landed on the plane at height 0** (S05): `GFXGPU`'s `GetViewVolumeCrosses` was empty.

### Decisions (S06)

- **One rule, shared.** `NCameraTurn` (`Main/CameraTurn.h`) gains `FacingCounts()` (Remastered on, or the Map Editor:
  the `editor` var its bridge sets), `IsFourSided` and `FourSidedPath` (moved from `MOObject.cpp`), and
  `StandingTurn(path, building, direction)`: the quarter turns an object stands turned by, its facing where facings
  count and its picture turns with it. The Game's drawing, the AI and the editor bridge all call these.
- **Where turned art must exist:** a building turns only to a side its variant has pictures for (`<variant>_r<k>\1.san`,
  `ModelPath`'s own test); the editor refuses any other turn with `"<name>" has no pictures of that side (the Resource
  Editor's Generate sides makes them)`, so it never shows a footprint its picture does not match. Facing 0 always
  exists. A map that holds such a facing anyway (written by hand) draws and lays the building as in 2003 (its picture
  falls back, and `StandingTurn` is 0). A prop of four sides always has its four variants.
- **The map stores the facing as the object's `Direction`**, a multiple of 16384 (0 faces +y, a quarter is
  `(x, y) -> (-y, x)`, the side convention). Q and E step by a quarter on a building or a prop of four sides
  (`BkEditorTurnStep`, asked by the Selector and the Placer), a sixteenth on everything else; the bridge takes the
  nearest quarter of any turn of such an object (the direction wheel, the Properties angle). The Place tool's ghost
  shows the side the placer's direction gives (`SetGhostInSession`). An object placed with a direction between
  quarters keeps that number in the map and is drawn and laid at the nearest quarter (`Facing`).
- **The footprint and points turn with the picture (S02's rough edge): turned at load, by bk-views' rule.** Rather
  than reading the `_r<k>` folder's turned `1.xml` (a second stats object per variant that the database does not
  know), `SObjectBaseRPGStats::GetTurned(k)` (overridden for buildings and props in `RPGStats.cpp`, where the stats'
  vtables live) makes a copy turned k times: the passability and visibility grids with their origins (`turnGrid`'s
  formula: cell (x, y) is cell (y, h - 1 - x), origin (h * TILE - oy, ox)), each entrance's and fire slot's position
  turned, each slot's direction plus 16384. The copies are owned by the stats they come from (`SStandingTurns`), so
  they live and die with the database. `CAILogic::AddObject` builds a building or a prop with
  `NStatsTurn::Turned(stats, NStatsTurn::TurnOf(desc, Direction))`; `CGivenPassabilityStObject::Init` turns the
  armour's front (`boundRect`'s axis) with it and `GetDir` answers the turn (16384 k), so a hit's side is taken from
  the turned front. Fire points, smoke points and direction explosions are the client's (effects) and are not turned.
- **When the AI turns:** when the map loads with Remastered on (the option is on the main menu only, so a mission runs
  in one mode), never in multiplayer (`NRemastered::Enabled()` is off there), a replay in its recorded mode. The
  drawing follows the option live (S02); a change of the option during a mission (the game's menus offer none) would
  change the picture and not the footprint.
- **Saves:** a building or prop standing turned writes the database's stats by name and its turn (chunk 41 of
  `CBuilding`, 5 of `CSimpleStaticObject`, only when not 0) and turns them again when read; the map Direction is chunk
  9 of `CGivenPassabilityStObject` (only when not 0). S02's chunk 41 of `CBuilding` (the map Direction) moved there.
- **The AI keeps a prop's map Direction (S03's rough edge)** for `SGVOGT_OBJECT` props, as for buildings; the client
  hears it through `GetPlacement` (new objects and placement updates alike) and a prop of four sides draws variant
  (own - 1 + facing + step) mod 4 + 1. Every prop's and building's sprite stays at direction 0, as 2003 had them.
  Fences keep their direction in their FrameIndex (the Fence tool sets it), as before.
- **The Map Editor's turn** (`CAIEditor::TurnObject` -> `CGivenPassabilityStObject::TurnForEditor`) unlocks the old
  tiles, swaps the stats for the turned ones, lays them again and tells the client (`ACTION_NOTIFY_ST_OBJ_PLACEMENT`;
  `CMOBuilding` and `CMOObject::AIUpdatePlacement` now redraw the side from the Direction). A turned footprint that
  would leave the map is refused, as a move is.
- **Clicks on hills (S05's rough edge):** `GraphicsEngineGpu::GetViewVolumeCrosses` is `CGraphicsEngine`'s: the
  inverse of viewport * projection * view at the time of the call, so `CScene::GetPos3` meets the terrain through
  `IAILogic::GetIntersectionWithTerrain` as in 2003, at every step, step 0 included. The Map Editor keeps its picks on
  the plane at height 0 (`ScreenToWorld` asks `GetPos3` with `bOnZero`), as it always did: its brush outline and
  `WorldToScreen` are drawn at z = 0.
- **Harness:** BK_AUTO_UI `ground=<x>x<y>` (`TraceGround`, the ground point a click there finds); the editor's
  `do=camera_at:<x>:<y>` (the view centred on a world point).

### The tiers (S06)

`zig build camera-turn-editor-game-reads-it` (`tools/zig/camera_turn_editor_game_reads_it.zig`, about 7 minutes on
win-home): bk-views makes the turns of e_ctownhouse01_2 and e_frenchshed01 into a scratch mod; a map from road3d.xml
holds two town houses, the shed and Bench01 at facing 0, each 300 AI units along both axes from the middle (no picture
comes near another at any step). The installed Map Editor (hidden, BK_EDITOR_AUTO, the mod loaded) selects each
(`band_select`), turns it with Q and E (the town houses to facings 2 and 3, the shed and the bench to 1, `expect=angle`
after each), places E_House01_1 (no turned pictures), presses E and must refuse ("pictures" on the status line, angle
0), deletes it, takes the frame around each object and saves the map as XML (Check Map clean: the objects belong to
the neutral player 2). The Game then plays the saved map:

- **editor:** each object's side is found in the editor's frame (the window with its panels and the ground) by its
  colours: mean colour difference 0.00, 0.14, 0.00, 0.00 for the two town houses, the shed and the bench; any other
  side at the same place 34 to 58. The saved map holds Direction 32768, 49152, 16384 and 16384.
- **on** (Remastered on, the camera on each object, steps 0 to 3): every object shows turn (facing + step) mod 4: the
  town houses IoU 0.998 to 1.000, colour 0.00 to 0.18; the shed 0.994 to 1.000, 0.00; the bench (its family's variant
  of that turn) 0.946 to 0.995, 0.00 (floor 0.9 for a prop, whose outline is a few pixels of edge); the best other turn
  0.16 to 0.47.
- **off** (Remastered off): every object shows its 2003 picture (IoU 0.954 to 0.998, offset within a pixel) and the
  turn key changes 0 pixels between the message line and the panels.
- **ai** (a sniper added to a copy of the saved map; the option set before the map loads, by the drawing run of that
  mode in the same profile): ordered along a line 50 AI units in front of the shed, which crosses the shed's 2003
  footprint and misses its turned one, then into the shed:

  | | largest distance from the line | samples in the 2003 / turned footprint | entered at, from the nearest turned / 2003 entrance |
  |---|---|---|---|
  | on | 10.2 | 15 to 17 / 0 | 0.9 to 30.5 / 72.8 to 93.4 |
  | off | 71.0 to 72.4 (it walks around) | 0 / 9 to 10 | 93.0 to 116.7 / 0.8 to 31.4 |

  The footprints are the AI tiles the shed locks (its 1.xml origin in AI units on 32-unit tiles: 2003 x -224..0,
  y -8..120 from its place; turned x -128..0, y -232..-8). A soldier goes in within a tile of the entrance it walked
  to (floors: 40 to its own side's, 60 from the other side's). The values are the spread over five runs; the last,
with another checkout's Game tiers running on the same machine, took 17 minutes instead of 7.

`camera-turn-control-game-reads-it`'s hill run gains clicks: with each anchor trace, the ground at the screen's centre
and at a point on the slope (611, 333). Measured: the centre's ground 1.9 to 2.7 world units from S05's
`GroundUnderCentre` (the AI meets the terrain in its own height steps); the slope's ground drawn by the turned 2003
camera within 1.1 to 1.8 pixels of the click, at height 63.6, 55.6, 12.1 and 3.6 at steps 0 to 3 (the plane answered
0 at every point). At step 0 a sniper ordered to the slope point walked to 3.6 to 6.8 world units from the clicked
ground, 103 to 105 from the point at height 0 the port answered before.

### Other tiers (S06)

Run on win-home (Windows x64, debug), one at a time, after the last engine change but the save fix (which only skips
re-turning stats saved at turn 0): `camera-turn-game-reads-it` (391 s), `camera-turn-world-game-reads-it` (448 s),
`camera-turn-units-game-reads-it` (320 s), `camera-turn-state-game-reads-it` (454 s) and `building-views-game-reads-it`
(295 s) pass. `remastered-game-reads-it`: 13 of 17, the 4 failures the known hb-view `l10n_measure` tests; its first
run (before the save fix) hung in the `save-off` Game, whose game time stood at 50 ms from the mission's start (the
main thread drawing, no load and no assert), and was killed after 25 minutes; the run after the fix passed in 7 minutes.
Map Editor: `test-editor-core`, `test-map-editor-view`, `-panels`, `-auto`, `test-map-editor-engine` (351 s),
`map-editor-host-check` (219 s), `map-editor-smoke` (109 s), `map-editor-auto` (158 s), `map-editor-m3-auto` (1436 s,
with the M1 and M2 scenarios it waits for) and `map-editor-game-reads-it-m3` (1303 s, with its siblings) all exit 0.
RPGStats.h changed for every module: `install-resource-editor` builds, `resource-editor-smoke` passes (64 s).
`resource-editor-auto-bdg` passes (95 s). `test-editor-bridge` in its five CI shards (the bridge changed): 841, 1421,
1046, 501 and 1456 s, all exit 0 (each longer than one 10-minute foreground command, so run in the background and
waited for, one at a time).
The editor's UI text did not change (the refusal is the bridge's message, which the lang files do not hold), so the
ten lang files and the l10n tiers were not touched.

### Rough edges left (S06)

- **Armour sides and fire slots** turn by construction (`boundRect`'s axis, the slots' positions and directions) and
  are not measured in a capture; the observation places a building computes from its slots follow them.
- **A building's client-side points** (fire, smoke and direction explosions, a slot's gun flash, drawn from the
  database's stats) stay where the variant's own 1.xml puts them, at every camera step (as in S01 to S05).
- **The Map Editor's turned buildings in the editor itself:** the AI's turrets and observation places of a building the
  editor turns are not rebuilt (nobody fires in the editor; the Game builds them turned when it loads the map).
- **Mid-mission option changes** (no game menu offers one) would turn a building's picture and not its footprint.
- **The editor's picks stay at height 0** (its own convention); only the Game's clicks meet the terrain.
- A soldier enters a building within a tile of its entrance (0.8 to 31.4 AI units measured), so the tier's entrance
  floors are a tile wide.
- `remastered-game-reads-it` hung once (above), not seen again; nothing in S06 is known to cause it.

## The maintainer's first hand test (S07, 2026-10-27)

Branch `fix/camera-turn-hand-test`, made from main right after `feat/building-views` was merged (94a6b6d70). The
maintainer played the build (Remastered on, the camera-turn mod) and reported four problems with screenshots, plus six
known rough edges. Brief item 4 (the barn's ramps, bk-views' generator and its gate) went to another agent in its own
worktree; it is not in this section.

### Step 0: the merged tree (S07)

On win-home (Windows x64, debug install), one at a time: `install-game install-map-editor install-resource-editor` and
`l10n-check` build. `test-l10n-check`: 81 of 82; `fit_font_missing` expects `no-fonts/SHA256SUMS` and bk_text names
it with Windows' separator (`std.fs.path.join`). The test and `BkText` are byte for byte main's (the merge took them
unchanged; CI runs this tier on Linux only), so it is not merge breakage; left as found. `camera-turn-game-reads-it`
(389 s), `building-views-game-reads-it` (290 s) and `replay-game-reads-it` (455 s) pass. `remastered-game-reads-it`
with hb-view on PATH: the first run 16 of 17 (the `load-on` Game's click on the truck after loading an OFF save
selected nothing), the rerun 17 of 17, the four `l10n_measure` tests passing for the first time on this machine. No
`fix(merge)` commit was needed.

### Item 1: "buildings do not turn" (S07)

- **Where the screenshots come from.** The village (a church with a clock and a tower, red half-timbered houses on a
  bending road) is Poland's (the first German mission; tutorial1 has the same map) at the half turn: Poland's village
  at step 2 is that picture, house for house. The camp with barbed wire, watch towers and tents is the multiplayer map
  deathzone. Found with `bk-map to-xml Data/Maps` and the objects.xml names of the buildings on the contact sheet.
- **What the Game does.** Every one of the village's buildings draws its `_r<k>` pictures at every step on Windows:
  `camera-turn-village-game-reads-it` turns four of them (e_jettybuilding_4, the church; e_house02_2, e_house01_2,
  e_house09_1) with `bk-views side` into a scratch mod and finds each one's own turn at every step, better than any
  other turn of it, at its turned place (below). The lookup is case-safe: the mod storage is a common file system
  that lowercases every name it enumerates and every name it is asked for, so `buildings\Europe\Summer\E_House02_2_r2`
  finds `buildings/europe/summer/e_house02_2_r2` and any other spelling. `test-map-files` now proves it under an exact
  comparison: a fixture mod (`tools/zig/fixtures/camera-turn-paths`) holds `Buildings/Europe/Summer/E_Test_1_r1` and
  `.../e_test_1_r2`, and `NCameraTurn::ModelPath` finds turns 1 and 2 and falls back at turn 3 (CI's Linux file system
  compares names exactly).
- **What remains is the pictures.** What reads as "the same side at every step" is how M005's generated sides look:
  a wall or roof plane no 2003 picture sees borrows the seen opposite one, so the back of a house shows its front's
  chimney, dormers and gable again, and e_jettybuilding_4's half turn shows the clock gable where its cross wing should
  come towards the viewer. Their outlines do differ (another turn's IoU at the same place 0.19 to 0.40), so the engine
  is not showing one picture. That is bk-views' generator, the other agent's scope (with item 4).
- **Fixed in the engine:** a turn the data holds no pictures of was drawn as the building's own side without a word,
  which looks exactly like a mod not loaded or a batch not run. `CMOBuilding::GetModelPath` now writes
  `camera turn: no pictures of <variant>_r<k> (its turn k); drawn as its own side` to the log, once per variant and
  turn.

### Item 2: "positions off, differently per resolution" (S07)

- **The buildings are exact.** `camera-turn-village-game-reads-it` measures each house's zero cross against the turn
  of the 2003 camera (an offset (dx, dy) from the centre (w / 2 - 1, h / 2 - 1) goes to (2 dy, -dx / 2) per step) in
  windows of 1024 x 768, 1280 x 1024 and 1920 x 1080: the best place within 6 pixels is the predicted one, offset 0,0,
  for all four houses at all four steps at all three sizes (IoU 0.993 to 1.000, colour 0.00 to 2.08).
- **The ground was not.** Its patches are laid from the map corner's projection cut to a pixel (`int`), and with the
  camera snapped that place is a whole pixel plus a float error either side (traced in 1024 x 768: 812.0003, -128.0001
  at step 0; 211.9998, 896.0002 at step 2; the two add up to the screen's size, a half turn being the reflection about
  (w / 2, h / 2)). At the half turn the cut fell on the other side of the pixel at some sizes: the ground stood a pixel
  low in 1024 x 768 and right in 1280 x 1024 and 1920 x 1080 (9 x 9 block difference to step 0 reflected 0.97 at no
  offset, 0.11 one pixel down; 0.08 at no offset in the other sizes). So houses stood a pixel off their ground, at some
  window sizes only.
- **Fix:** at the half turn the corner is laid on the 2003 cut's pixel reflected, `nScreen - 1 - int( nScreen - place )`
  (`GroundPixel`, `Scene/TerrainInternal.cpp`, for the patches and the war fog), which reproduces the 2003 cut's error
  instead of making its own. Rounding to the nearest pixel was tried first and measured worse (a pixel off at every
  size), and so was `ceil( place - 0.001 ) - 1` (1920 x 1080 a pixel high): the error is real float error up to a few
  thousandths at these magnitudes, and only the reflected cut agrees with it everywhere.
- **Tier:** `camera-turn-ground-game-reads-it` (new step, same file) shoots river3d's ground alone at the three sizes:
  step 2 against step 0 reflected, best offset 0,0 with 0.08 to 0.09 at all three. The odd steps are reported only:
  their tiles show pictures drawn upright for the screen, so a reflection between steps 1 and 3 does not reflect the
  grain and no offset stands out (2.5 to 2.8 everywhere, within 0.15 of each other).

### Item 3: "fences hang over in the opposite view" (S07)

- **Cause.** A 2003 fence segment's zero cross is at one end of its footprint, not its middle: every shipped fence's
  centre segments are 1 x 3 cells with origin 16, 16 (directions 0 and 2) or 3 x 1 with origin 80, 16 (1 and 3), AI
  units, so the footprint's middle is (0, 32) or (-32, 0) from the zero cross (the two hedges and the metal net are
  half a cell off that). S03 drew the segment of direction d + k at the own segment's zero cross; at steps where the
  turned own middle and the drawn segment's middle differ (the half turn for every direction, and one odd step) the
  picture ran from the zero cross the other way from the footprint: a whole segment off its line.
- **Fix:** the drawn picture moves by R^-k of (R^k m(d) - m(d + k)) (`CMOObject::ShiftPicture`, the footprint middles
  from the segment's passability and origin), so its footprint's middle lies on the own one's turned; placement,
  `GetPlacement` and a damaged segment's new frame keep the shift (saved as chunk 10 while turned).
- **Tier:** `camera-turn-world-game-reads-it` now expects each VillageFence segment's picture on its own footprint
  turned (the shift from the fence's 1.xml, `fenceShift`, unit-tested): IoU 1.000, colour 0.00, offset 0,0 at every
  step; the S03 placement, where the shift is not 0, scores 0.061 to 0.097. The ends of a segment are on the turned
  line because its footprint, which is the line, is laid on the turned footprint.

### Items 5 to 10 (S07)

- **5, fire, smoke and flashes.** A building's points (1.xml `WorldPosition`) are 3D places on the building, so the
  camera step needs nothing: the turned view draws them where the turned picture shows that part. What was wrong is a
  building standing turned by its own facing (the Map Editor's Q and E): its points stood at the variant's unturned
  places. They are now turned by its standing turn (`NCameraTurn::StandingTurn`, as the AI turns its footprint and
  slots), their directions with them, and a slot's flash-or-sound choice reads the slot's direction as the view sees
  it. Not captured (an effect's particles are not placed by a tier yet).
- **8, bridge begin and end spans.** Tried: lay the drawn span's footprint middle on the own span's turned, as for
  fences. Dropped: a shipped bridge is a begin and an end span at the same point (dessau's AsphaltBridge_01 and _02:
  frame 1 and frame 4 at one place), so the spans meet at that point in the river, and S04's choice (the drawn span
  from the shared point) keeps that junction exact and leaves the length difference of the 2003 variants (up to a
  tile) at the outer end, on the bank. Centring splits that difference between the junction and the bank, and in
  the dessau scene at the half turn the near railing of the bridge across the river then lost half its run; with
  S04's placement back it is whole (`camera-turn-world-game-reads-it`'s line spans unchanged either way: 0.950 to
  1.000). Open: only spans of the right length (generated ones) make both ends fit.
- **6, armour sides and firing positions:** not measured in this slice (open). The measure planned: a trace of a
  turned building's turned slots, entrances and armour axis as the AI builds them, compared with `bk-views`' `turnXml`
  of the variant's 1.xml (an independent implementation of the same turn).
- **7, relighting mirrored props and terra objects:** open. The engine has no light information for a sprite to
  relight it with, and bk-views makes sides from building 1.xml and packs only; either is a slice of its own.
- **9, the Map Editor's Q and E on fences:** open. A fence's direction is its FrameIndex, which the bridge has no call
  to change in place (the Fence tool deletes and lays fences); it needs a bridge call and the Selector's key.
- **10, multiplayer:** open. The Game has `-host` and `-connect` (GameSpy staging room), but a two-Game harness would
  have to drive the staging room's screens through BK_AUTO_UI; not done here. The proof stays the replay one (S05):
  the step is in no command and the network stream carries only commands and checksums, so two players at different
  steps run the same commands.

### Item 2, second report: the turned village crowds (S07)

The maintainer's second screenshot (current main, one turn of Poland's village with `.`) shows the church over the
barn and houses crowding, while trees, fences, wells and the road look right.

- **The engine places every picture right.** `camera-turn-poland-game-reads-it` lays seven of the village's buildings
  (the church e_jettybuilding_4, the barn e_wb/01, e_house09_1, e_house03_4, e_house02_1, e_house02_2 and the
  wayside cross e_stella/02) on road3d as Poland holds them, turns each with `bk-views side`, and turns the camera
  with `.` and with `,` in 1024 x 768 and 1920 x 1080: every picture's best place within 8 pixels is its map place
  turned (results below). Poland's three houses 470 to 560 units further north are left out (the village would not
  fit on road3d with room for its footprints).
- **The pictures stood off their footprints.** The 2003 camera cannot tell a building moved along its line of sight
  on the ground (world (1, -1)) and lifted by as much: both draw the same picture. bk-views' fit (and the hand shapes
  made from it) leaves the blocks anywhere on that line; turned, that error is no longer along the line of sight. The
  church's blocks stand (-20, +16) world units off its footprint's middle, 18.2 along that line: invisible at step 0
  (2.5 pixels across, the rest reads as height), 25 pixels sideways after a quarter turn, into the barn.
- **Fix (bk-views):** a turn drawn by the blocks gets its zero cross where the blocks would stand moved along that
  line by what puts their ground box's middle nearest the footprint's middle (`footprintZero`); the rest of the
  offset is the 2003 picture's own and kept. 2003 sprites, mirrored sprites and 2003 siblings keep their zero cross.
  Measured on the europe batch: the church's zero cross moves 26 pixels at odd turns and 13 at the half turn, the
  barn's 12 and 6, the houses' 0 to 8; in the Game the church at step 1 now stands clear of the barn.
- **Keys:** `.` turns the view so the world turns counter-clockwise on the screen (a step k shows (x, y) -> (-y, x)
  k times: the camera goes round clockwise); `,` turns it the other way. The Poland tier presses each and finds every
  building where that direction puts it.
- Seen on the way: E_WB_01 (a storage) placed for no player (Player -1) traps the debug Game at load (illegal
  instruction, a UBSan trap) on a map of its own; Poland's are player 0 or 1. Not chased (not the camera turn).

### The minimap's north mark (S07)

The frame's `S -> N` on its upper left bar (painted in `ui\missionpanels`) points at the map's north, world +y (up and
right at step 0). The minimap turns with the view, so north on the screen is +y turned k times: up and left at step
1, down and left at 2, down and right at 3. `CUIMiniMap::DrawNorthMark` draws the mark's band from the texture (Remastered
steps only; step 0 draws nothing, the 2003 frame):

| Step | North on the screen | The mark |
|---|---|---|
| 0 | up, right | the 2003 frame |
| 1 | up, left | on the upper right bar, mirrored (N <- S); the left plate's letters painted over in its yellow |
| 2 | down, left | on the upper left bar, turned half round |
| 3 | down, right | on the upper right bar, upside down; the left plate painted over |

The band runs between (52, 26)-(52, 35) and (86, 9)-(86, 18) of the texture; the right bar is the left one mirrored
about x = 135 (the minimap window stands at (6, 3) of the panel, whose top left is the texture's). Measured by
`camera-turn-ground-game-reads-it` in its 1024 x 768 shots (the interface is scaled in the larger windows): the band
where the turn puts it differs from step 0's by 14.1, 14.9 and 15.0 (mean RGB, the texels resampled) against 36.3 to
42.3 left where it was, and at odd steps none of the 77 dark letter pixels stays on the left plate.

### Tiers run (S07)

On win-home (Windows x64, debug install), one at a time, after the last engine change but the north mark (UI only),
another worktree's bk-views tiers sharing the machine at times:

| Tier | Result |
|---|---|
| `camera-turn-world-game-reads-it` | pass (450 s; fences at their footprints, bridges as in S04) |
| `camera-turn-village-game-reads-it` (new) | pass (250 s) |
| `camera-turn-ground-game-reads-it` (new) | pass (195 to 227 s, with the north mark) |
| `camera-turn-game-reads-it` | pass (391 s) |
| `camera-turn-units-game-reads-it` | pass (246 s) |
| `camera-turn-control-game-reads-it` | pass (311 s) |
| `camera-turn-state-game-reads-it` | pass (442 s) |
| `camera-turn-editor-game-reads-it` | pass (473 s) |
| `building-views-game-reads-it` | pass (438 s) |
| `remastered-game-reads-it` | pass, 17 of 17 (485 s) |
| `test-map-editor-engine map-editor-smoke` | pass (299 s) |
| `test-map-files` | pass (with the case test) |
| after the bk-views zero cross: `test-bk-views bk-views-side` | pass (338 s) |
| after it: `building-views-game-reads-it`, `camera-turn-village-game-reads-it`, `camera-turn-game-reads-it` | pass (297, 257, 394 s) |

`camera-turn-poland-game-reads-it` (new, 1118 s with the machine shared): pass. Of 112 pictures (seven buildings,
four shots, `.` and `,`, 1024 x 768 and 1920 x 1080) 108 are found: 84 at the turned place to the pixel, 24 a pixel
off (12 at step 0 too, where the Game draws 2003's: the map place lands between pixels and the prediction rounds it
the other way), colour difference 0.0 to 19.4 (neighbours in front); the wayside cross is hidden behind the church
at the half turn (4 shots). `.` and `,` give the same picture for the same step.

The full sweep is left to the maintainer; Linux and macOS to CI. The batch output in `zig-out/remastered/Buildings`
was regenerated for europe only (`building-views-batch-europe`, then `camera-turn-mod`); run the other three regions'
batches and `camera-turn-mod` again before playing.

## What the first hand test left open (S08, 2026-10-28)

Branch `fix/camera-turn-open`, made from main at f98294c83 (S07 merged). Each item: the cause, the fix and the tier that
measures it. The bk-views generator (`tools/zig/bk_views.zig`'s side, fit and gate code) belongs to another agent and is
not touched here.

### 1. A storage placed for no player (S08)

- **Seen in S07:** E_WB_01 (a barn, `BuildingType` 1, a storage) with `Player="-1"` trapped the debug Game at load.
- **Reproduced** on road3d with the barn alone: player -1 exits 0xC000001D (a UBSan trap), player 0 loads. The option was
  off at load, so it is pre-existing (the 2003 code path).
- **Cause** (caught in cdb): `CAILogic::AddObject` hands the map's player to `CStaticObjects::AddNewStorage`;
  `CStoragesContainer::AddStorage` asks `CDiplomacy::GetNParty( BYTE )` for player 255 and indexes `playerParty` (players
  0..n) past its end. `IsPlayerExist( -1 )` would read before it.
- **Fix:** a storage of a player outside the diplomacy's table is the neutral player's (`GetNeutralPlayer()`), whose party
  2 `AddStorage` already leaves out of the supply lists.
- **Tier:** `camera-turn-storage-game-reads-it` (`tools/zig/camera_turn_open_game_reads_it.zig`): the barn of Player -1
  on road3d, Remastered off; the Game must exit 0 without a leak and draw it (19 271 pixels drawn around the middle).

### 2. Armour sides, fire slots and flashes of a building standing turned (S08)

- **The trace:** `BK_FIRE_TRACE=1` prints where the AI puts a soldier to fire (`CBuilding::SetFiringUnitProperties`: slot,
  building, its standing turn, the soldier's place, the slot's direction), the side every shot hits
  (`CExistingObject::GetRandArmorByDir`: attack direction and `SRect::GetSide`) and where the client draws a slot's flash
  (`CMOBuilding::AIUpdateShot`).
- **The AI was right.** `SBuildingRPGStats::GetTurned` turns the slots' places and directions and
  `CGivenPassabilityStObject::Init` the armour's front; the tier finds every soldier on its slot turned and every hit on
  the turned side.
- **The client was not:**
  - **Flash, fire and smoke points.** A 1.xml point has two places: `Position`, its ground place, and `WorldPosition`, the
    picture point taken back to some depth on the 2003 camera's line of sight. Over the 5 710 points of all shipped
    buildings the two lie a median 43 world units apart, 153 at the 90th percentile, 405 at most; both give the same
    picture point (screen x to 0.001 px for 5 709 of them). The 2003 view draws `WorldPosition` at the window, so S07's
    "the points are 3D, the camera step needs nothing" was wrong: turned, a flash or a fire is drawn off the wall.
  - **Fix:** `CMOBuilding::PointOf`. With the Remastered option, the point is the one on the same line of sight above
    the ground place: the 2003 camera puts world (x, y, z) at picture y (x - y) / (2 sqrt 2) - z sqrt 3 / 2, checked to
    0.001 px on all 5 710 points, so the point at `Position`'s x, y is raised by the change of x - y over sqrt 6. The
    solved heights are sane: median 1.2, 95th percentile 94.7, 216 for the city hall's tower, -16.7 at the lowest. It is
    then turned by the standing turn. Without the option it is the 2003 point, as before.
  - **Radians:** S07 added the standing turn to the fire and smoke points' directions as 90 degrees a turn, but
    `ToAIUnits` has made them radians: now pi / 2 a turn.
  - **Direction explosions** (`AIUpdateHit`) chose and placed the 2003 explosion point of the unturned building. They
    now choose by the direction turned with it and draw at `PointOf` with the direction turned.
- **A leak found on the way** (pre-existing): `PlayEffect` and `SetFlashEffect` (`Common/PlayEffect.h`) built an effect
  and lost it when the scene refused it. `CScene::AddEffectObject` refuses every effect while effects are off: a
  player's own switch, or the tiers' `BK_SCENE_DISABLE=effects`. A building under fire leaked 1 310 to 1 953 blocks at
  exit, Remastered on or off. The objects are now held in a `CPtr` while they are added: 0 leaked blocks.
- **Tier:** `camera-turn-fire-game-reads-it` (same file). E_FrenchShed01 at its own facing f = 0..3 (Direction 16384 f,
  Remastered on before the map loads, its turns made by `bk-views side` into the scratch mod), a German squad linked into
  it. Around it, placed turned with it: a T-34 in front of its own front (+x), one at its left (+y) and a Soviet squad at
  its right (-y). 24 s of game time each.

  | facing | soldiers put to fire (worst off the slot turned, direction off) | hits on the expected side (front, left, right) | what the unturned front would give | flashes: ground from the soldier, picture point from 1.xml's |
  |---|---|---|---|---|
  | 0 | 19 (1.34 AI units, 0) | 57 of 57 (23, 20, 14) | the same | 9: 0.01 AI units, 0.90 px |
  | 1 | 23 (1.34, 0) | 39 of 39 (17, 1, 21) | sides 1, 2, 0 | 4: 0.01, 0.90 |
  | 2 | 19 (1.34, 0) | 27 of 27 (4, 3, 20) | sides 2, 3, 1 | 14: 0.01, 0.90 |
  | 3 | 24 (1.34, 0) | 18 of 18 (3, 13, 2) | sides 3, 0, 2 | 9: 0.01, 0.90 |

  The 1.34 AI units and 0.90 px are the same at facing 0: the AI holds a slot's place in whole units against the 1.xml
  float. The floors are 1.5 units and 1 px. Each flash's ground point is the firing soldier's, and its picture point,
  unturned and drawn by the 2003 camera, is the slot's 1.xml `PicturePosition`. The 2003 `WorldPosition` of the slots
  that fired lies up to 108.6 AI units from the soldier.

### 3. Relighting mirrored props and painted terra objects (S08): planned, not built

- **Mirrored props** (97 SimpleObjects and 5 trees on a longer rectangle, S03) draw their 2003 picture mirrored at odd
  steps, so the lit side flips. **Painted terra objects** (ravines, dunes, mounts, coasts) turn flat with the ground and
  keep their relief lit from the turned side. Nothing in the data says which pixel is a lit face.
- **Considered:**
  - *A lit/shaded mask from the sprite's own shading against its mirror.* At an odd step the mirror would take the
    brightness of the unmirrored picture at the same screen pixel. The sprite pass draws a picture as squares of an
    atlas, mirrored square by square (`SBasicSpriteInfo::bMirror`), so the unmirrored texel at a screen pixel can lie in
    another square. That needs a second sampled texture per sprite in the GPU sprite shader, or a relit texture made at
    load, for every mirrored prop: not short.
  - *Generated sides by bk-views*, as M005 relights a mirrored building by the blocks' shading classes: bk-views' `side`
    reads building 1.xml and sprite packs only, and S07c's `props.json` lists building variants drawn as their own
    sprites. A prop needs the object's 1.xml (passability as the footprint, a box fit), its turns
    `<object>_r<k>` in the mod, and `CMOObject` drawing `ModelPath` as buildings do instead of the mirror.
- **Decision:** generated sides through bk-views, the same path as buildings and one look across the game. The side
  command's input reader is the generator agent's file, so it is planned here, not done:
  1. `bk-views side` takes a `Data/Objects/SimpleObjects/<...>` folder (its 1.xml `passability` and `origin`, a box fit to
     the 2003 outline);
  2. `camera-turn-mod` copies the turns;
  3. `CMOObject::ApplyCameraTurn` draws `<object>_r<k>` where the data holds it, the mirror only as the fallback;
  4. a tier measures the lit side's position: the brightest face's centroid against the step-0 frame turned.

  Painted terra objects then follow with a relief shade turned with the step (a later slice).

### 4. Bridge begin and end spans (S08): no 2003 pair fits

S07 found that centring a span moves the length error into the river junction. A shipped bridge is a begin and an end
span at one point. The 1.xml `Spans` (`Length`, tiles) of every kind with both variants give no choice: each variant
holds exactly one begin and one end span (intact state, `Begins` and `Ends` one entry each).

| kind | `_01` begin + end | `_02` begin + end |
|---|---|---|
| woodenlittle (and w_) | 5 + 7 = 12 | 7 + 7 = 14 |
| asphaltbridge (and _special, w_) | 8 + 11 = 19 | 10 + 10 = 20 |
| railwaybridge_little (and w_) | 12 + 13 = 25 | 12 + 11 = 23 |
| woodenbig_heavy (and w_) | 10 + 10 = 20 | 11 + 10 = 21 |

- At an odd step the other variant's pair is 1 to 2 tiles longer or shorter than the own pair turned. So no choice of
  spans and no shift of the junction lays both banks right: the error can only be moved, to the bank (S04's
  placement, kept) or into the river (S07's centring).
- At the half turn the own pair is drawn with its ends traded, and begin and end differ by 0 to 3 tiles.
- **Decision:** spans of the right length need pictures that do not exist in 2003. A begin or end span of the other
  variant's length, made by bk-views from the span's blocks (M005's pipeline taught to read bridge segment packs), is
  the fix. It is left to a slice that may change the generator. S04's placement stays: the junction is exact and the
  bank end is off by up to two tiles.

### 5. The Map Editor's Q and E on fences (S08)

- **Cause:** a fence's direction is its packed frame type `(1 << dir) | 0x10000`, which the engine reads when it builds
  the fence. Q and E changed the map Direction, which a fence ignores, and the bridge had no call to change the type.
- **Fix:** `BkEditorTurnFence( session, link_id, quarters, out_token )` (`TurnFenceInSession`, `session_groups.cpp`):
  - the fence goes out and comes back as the centre segment of its direction `quarters` on, one logged edit (an
    `SGroupEdit`, as a fence run);
  - it keeps its link ID, owner, HP and script ID;
  - it is laid so its footprint's middle stays where it was: the Game's rule for a fence it draws turned
    (`CMOObject::ShiftPicture`, S07), so the turned fence stands on the line the Game showed turned;
  - a fence the engine will not place there is refused, changing nothing.
- The Selector's Q and E call it for an object whose type is one of the database's fence types (`Editor.isFenceType`,
  through `BkEditorFenceDescriptors`); everything else turns as before. The fake bridge turns the packed type and keeps
  the place.
- **Tests:**
  - `test-editor-core`: Q and E on a fence step its packed type 1 -> 2 -> 1 -> 8, Direction 0 throughout, and undo
    brings it back.
  - `test-editor-bridge`: a single W_FactoryFence turned E, E, Q, E, E, E. Each turn keeps the link ID, takes its
    direction's packed type, and the engine's world agrees with the map. Four quarters on it is back at its place, a
    missing link ID is refused, and undoing every turn and the fence saves the unedited bytes.

### 6. Multiplayer: two players at different camera steps (S08)

- **Feasible from the command line.** `-host` opens the create screen. BK_AUTO_UI `msg=10004` presses its Create
  button (the first multiplayer map, the screen's own choice), and `msg=10005` later presses the room's Launch. On the
  other Game, `-connect127.0.0.1` joins the room and `msg=10004` presses Ready.
- **What stood in the way** (found by running the pair):
  1. **The port.** Every player's in-game driver takes UDP port 8889, so two Games on one machine collide. The net
     layer answers the sender's own address, so `BK_NET_CLIENT_PORT` (a harness seam) gives the joining client
     another port.
  2. **Creating a room threw.** `NWin32Helper::CCriticalSection` stands for a Win32 critical section, which is
     recursive, but the port backed it with `std::mutex`. `CNetDriver::StartGameInfoSend( SGameInfo )` locks and calls
     its stream overload, which locks again: `resource_deadlock_would_occur`, so the host ended (0xC0000409). It is a
     `std::recursive_mutex` now.
  3. **A UBSan trap on both players' net thread** once connected: `CStreamTracker::WriteMsg` read
     `SChannelBlock::GetHeaderSize()` through a null pointer (2003's `pMSVCSuck`). It is static now.
  4. **Leaks at exit:**
     - `CStatistics::SetFlagPoints` and `SetCapturedFlags` left their scenario iterators (raw pointers) every call;
     - `CGameSpyMultiplayer::InitJoinToServer` left its servers list and address.

     They are held in `CPtr` now. Over a minute of play each Game leaked about 2 600 blocks; now 0.
  5. **A mod** (`-mod=`) makes the room's Create switch the Game's mode instead of opening the room. The pair runs
     without one.
- **Tier:** `camera-turn-multiplayer-game-reads-it` runs the host, then the client 35 s later, on two threads, each in
  its own user folder with `BK_AI_STATE_TRACE`.
  - Each player starts at the step its corner gives (S05's start step): the host at 1, the client at 2.
  - The client turns with `.` and `,` while they play.
  - Every second of game time both logs hold (the multiplayer checksum's terms of every unit) must be the same for at
    least a minute, with at least 10 distinct states.
  - Both must exit 0 without a leak.
- **Measured before the tier's last schedule change** (by hand and by the tier, win-home, debug):
  - host steps 1, 1, 1; client steps 2, 3, 2;
  - 64 of 64 common seconds the same state, 28 distinct states;
  - 0 leaked blocks in both.

  An earlier pair (client turning 2 -> 3 -> 2) gave 55 of 55 the same, 55 distinct. The tier's run after the change:
  see "Tiers run (S08)": 90 of 90 seconds the same, 35 distinct states, host 1, 1, 1 and client 2, 2, 3, 0, 3, no leak.
- One tier run of the pair saw the client trap (0xC000001D) at its connect and was not seen again. A rerun under cdb
  and two more runs were clean (open, below).

### 7. `test-l10n-check` on Windows (S08)

`fit_font_missing` expected `no-fonts/SHA256SUMS`, and bk_text joins it with the host's separator (`\` on Windows). The
test now accepts either separator: 82 of 82 on win-home.

### Tiers run (S08)

On win-home (Windows x64, debug install), one `zig build` at a time and one job at a time within it (`-j1`), after the
last engine change, with another worktree's bk-views batch sharing the machine:

| Call | Result |
|---|---|
| `camera-turn-multiplayer-game-reads-it camera-turn-storage-game-reads-it camera-turn-fire-game-reads-it` | pass (1473 s) |
| `camera-turn-game-reads-it camera-turn-world-game-reads-it camera-turn-units-game-reads-it camera-turn-control-game-reads-it camera-turn-state-game-reads-it` | pass (2503 s) |
| `camera-turn-editor-game-reads-it camera-turn-village-game-reads-it camera-turn-ground-game-reads-it building-views-game-reads-it remastered-game-reads-it` | pass (2229 s; remastered with hb-view on PATH) |
| `camera-turn-poland-game-reads-it` | pass (884 s) |
| `test-editor-core test-map-editor-engine map-editor-smoke map-editor-auto test-l10n-check` | pass (799 s; the first try ran out of disk on C:, freed by deleting the earlier slices' by-hand scratch under `zig-out/local-test`) |
| `test-editor-bridge` shards 1 to 5 (`-Deditor-bridge-shard=k`) | pass: 857, 1279, 676, 501 and 1685 s |

The fire tier's last run: soldiers put to fire 21 / 14 / 16 / 22 at facings 0 to 3, hits 42 / 13 / 33 / 29 all on the
expected side, flashes 4 / 2 / 15 / 9 at 0.00 to 0.01 AI units from the soldier and 0.90 px from the 1.xml picture point.
The Map Editor's UI did not change (no new text: the Selector routes the same keys), so the lang files and the l10n tiers
were not touched. The full sweep is left to the maintainer; Linux and macOS to CI.

### Rough edges left (S08)

- Item 3 (relighting) and item 4 (bridge spans of the right length) need bk-views changes (planned above).
- One multiplayer tier run saw the client trap (0xC000001D) at its connect while another tier's Games shared the machine;
  three later runs (one under cdb) were clean, so the cause is unknown.
- The multiplayer pair runs on one machine at fixed waits (the host launches 80 s after creating the room); a slower
  machine may need longer waits. It plays the create screen's first map without a mod.
- No Game-reads-it tier places a fence turned in the Map Editor (the bridge test proves the map and the engine).

## The focus pause (round 2 of the hand test, 2026-10-28)

The maintainer's report (macOS arm64): the Game pauses when its window loses the focus, but artillery fire goes on and
is caught up after the focus comes back, and the frame rate measurement is not halted.

Causes:

- The macOS and Linux frame (`GameMain.cpp`'s `NWinFrame` over `GameFrame`) only cleared its active flag on a focus
  loss. `NMain::SwitchGame`, which holds `PAUSE_TYPE_INACTIVE` on the game timer and pauses the sound, was called only by
  `WinFrame.cpp`, which only the Windows build compiles (d71d2f5ba put the macOS fix there). So off Windows the game
  clock ran on and the AI fired its segments while nothing was drawn; the world caught up on the first frame back.
- The absolute clock (`IGameTimer::GetAbsTime`: chat lines, minimap marks, acknowledgements, the camera's slider, the
  harness's `t` keys) was never paused on any platform: it jumped by the time away.
- The mission's frame rate measurement (`CInterfaceMission::StepLocal`, the one that lowers the game speed under
  `Options.GFX.MinFPS`) counted the whole time away as one frame on the first frame back, on every platform: after a long
  enough pause the game speed went down a step with a "game speed lowered" chat line. The statistics overlay's frame
  rate (`CInterfaceScreenBase::AddStatistics`) did the same for one second.

Fix (Remastered on and off: this is 2003 behaviour in the port):

- Both frames call `SwitchGame` on SDL's focus lost and gained events.
- `CMainLoop::Pause` pauses the absolute timer with `PAUSE_TYPE_INACTIVE`, in single player only. Decision: in a
  multiplayer game the other players play on, and the lag and network timeouts are counted on this clock in real time.
- The mission's measurement re-anchors its last frame time on every unfocused step, and the statistics' timer is reset
  on every frame not drawn, so neither counts the time away.
- A Game driven by `BK_AUTO_UI` keeps ignoring the desktop's focus (decision: tiers run several Games and the desktop
  moves the focus between them); only the focus events the harness pushes itself pause it. The new action
  `blur=<ms>` pushes an SDL focus-lost event (`SDLApplication::PushFocusEventForTests`, `data1` = 1 marks it), holds
  the window inactive for ms of wall time and pushes focus-gained, so the frame's own focus code is what runs.
- New traces: `BK_SHOT_TRACE` (each `CGun::Fire` with its game time), `BK_FPS_TRACE` (each frame rate period of the
  mission with the game speed).

Tier `focus-pause-game-reads-it` (`tools/zig/focus_pause_game_reads_it.zig`): road3d with a Pz IV, a sniper and two
rifle squads against a T-34, `Options.GFX.MinFPS` at 10, the window unfocused for 8 s five seconds into the fight,
Remastered on and off. It holds game time and absolute time to move at most 100 ms across the pause, no shot fired
while unfocused (and shots before and after), and the frame rate period across the pause at least half the one before
and not lowering the game speed. Decision: MinFPS 10, not its highest 20 - a first run with other worktrees' builds on
the machine drew 14 to 18 fps from the mission's start and lowered the speed every period, pause or not.

Measured on win-home (Windows x64, debug install), 8 s unfocused: game time moved 42 / 31 ms and the absolute clock 42 /
31 ms (Remastered on / off), shots 3 / 11 before, 0 / 0 unfocused, 46 / 43 after, frame rate periods 47.4, 39.1, 48.0 /
46.1, 39.6, 35.4 at speed 0. The same tier with the absolute clock and frame rate fixes taken out: the absolute clock
jumped 8007 / 7982 ms, the period across the pause read 5.2 / 6.6 fps and lowered the speed to -1. The frame fix itself
(macOS, Linux) is not exercised on Windows, where WinFrame already paused; CI runs the tier there only if a workflow
names it. Also run: `test-game-frame test-game-loop test-game-bootstrap camera-turn-storage-game-reads-it` pass. Not
seen: macOS, Linux.

## The sun fixed in the world (M006 round 2, 2026-10-28)

The maintainer's second hand test: "Shadows are not moving. Light always comes from the left side. If we switch the
camera angle the shadows need to move, too."

### Decision (reverses S01/S04's light that turns with the camera)

- **The sun stays in the world.** S04 turned the sun with the camera so that meshes matched the 2003 pictures, which
  are all lit from the screen's left; M005's turned building sides were lit the same way. Now at camera step k the
  light and every shadow come from the world's sun as the turned camera sees it: from the screen's left at step 0, as
  in 2003, from the other side at step 2. Remastered off (step always 0) draws as before.
- **Vehicles and guns:** `CScene::Draw` hands the world's `sunlight` to mesh light and to `DrawShadow`; the turned
  local `sun` is gone. Measured: the T-34's colour against the step-0 tank at the turned place stays 0.01 to 0.15
  under either sun, so the GPU renderer does not shade meshes by the light's direction at all (`effectiveDrawColor`
  folds the material in, no Lambert term); their light is the texture's. Only their shadows show the sun.
- **Buildings: pictures per (side, step).** `bk-views side` takes `<k>l<s>`: turn k lit by the sun as camera step s
  shows it, `<variant>_r<k>_l<s>`, whose light is the building's facing k - s in its own frame (`Turned.light`, passed
  to `projectPicture`, `shadeClass` and the shadow). A face keeps the light the world gives it: a face the 2003 picture
  shows lit stays lit. Pictures a turn copies whole (a sibling's 2003 side, a thin object's own picture, a mirror) are
  relit pixel by pixel in place (`sunLit`: the blocks give each pixel's face, the light model gives the gain), so their
  2003 detail is kept; heaps are not relit, as before. The shadow is cast from the fitted blocks along the world's sun
  (`castShadow`). The batch makes `1l1`, `2l2` and `3l3` for every variant (the turns a building of facing 0 shows) and
  `bk-views report` lists them as `r<k>l<k>`. The Game (`NCameraTurn::LitModelPath`) draws `<variant>_r<d>_l<s>` at
  camera step s where the data holds it, its shadow with it; else `_r<d>` (the light fixed to the camera) and the
  shadow of step 0 laid turned with the ground (`GetShadowPath`). Step 0 of side 0 stays the 2003 picture.
- **Objects, fences, bridges: their own shadow turned with the ground.** A shadow on flat ground seen by the camera
  turned k quarters is the step-0 shadow turned with the ground about the object's zero cross: S03's flat turn
  (`SetScreenTurn`), no new data. Flora and props lay their own 2003 shadow so (a prop of four sides its own facing's
  variant's, not the shown side's), a fence segment its own segment's at its own footprint (no fence shift), a bridge
  part its own span's at its own place.
- **Flora and props lit by the world's sun** (the maintainer's follow-up: "relight them too"). `bk-views relight
  <objects-dir> <out-dir>` writes `<pack>_l<k>` beside each object's 2003 packs (every one-sprite pack but shadows;
  `1` and `1w`): each pixel's normal guessed from the outline as an upright body of revolution (across a row from the
  screen's left through the viewer to its right, the upper half of each column leaning up to 0.7; `spriteNormals`), the
  object's own lit against shaded side per channel (`objectLight`, held within the 13 houses' median), and each pixel
  relit by the 2003 light model to the light of turn -k (`relightSprite`), mirrored where the Game mirrors the prop.
  `CMOObject::ShowPicture` takes `<pack>_l<step>` where the data holds it (a prop of four sides its side's), else the
  2003 picture. `zig build objects-relight` makes them for Flora and SimpleObjects into
  `zig-out/remastered/Objects` (about 1 minute and 8 minutes on win-home: 195 and 1125 packs; the two mines are
  animation packs and keep their pictures); `camera-turn-mod` copies them into the mod.
- **Infantry: left as drawn.** A soldier's small shadow is drawn into its 2003 animation pictures (dark pixels at its
  feet), not by the shadow pass: the sniper shows 0 to 7 shadow-pass pixels. Turning it needs the shadow pixels split
  from the animation packs (`fmtAnimation`, not the sprite packs bk-views reads) and drawn flat-turned; its lit side
  likewise. Not done (below).
- **The minimap and fog** are not touched.

### The tiers (round 2)

`zig build camera-turn-sun-game-reads-it` (`tools/zig/camera_turn_sun_game_reads_it.zig`): a map from road3d.xml with
a T-34, the lime tree `_Lime` and a German sniper on a ring (eighths 0, 2, 4) and `E_FrenchShed01` on a smaller ring
(eighth 6, clear of the pause label), the shed's turns made with `1,2,3,1l1,2l2,3l3` and the tree relit into a scratch
mod. A shadow is where the frame with shadows is darker than the one without (by 30 over three channels); each
object's shadow at step k is scored against its own step-0 shadow mapped by the ground's turn about it, on the ground
neither picture of the object covers (`sunShadow`): IoU at the best offset within 4 pixels, against the same not turned
(the sun turning with the camera), and the angle between the two shadows' centroids on the ground (screen y doubled).
Held where the shadow shows 100 pixels or more: IoU 0.35 or more, above the not-turned IoU, centroids within 30 degrees,
and at one step at least per object. Measured (win-home, Windows x64, debug install):

| step | shed | T-34 | lime |
|---|---|---|---|
| 1 | hidden behind it (0 px) | 0.650 / 0.232, 3.2 deg | 0.749 / 0.239, 7.7 deg |
| 2 | 0.380 / 0.000, 15.7 deg | 0.461 / 0.169, 9.8 deg | 0.991 / 0.218, 0.2 deg |
| 3 | 0.647 / 0.000, 7.4 deg | 0.810 / 0.205, 13.2 deg | 0.861 / 0.239, 0.4 deg |

(IoU turned / not turned, centroids apart.) The shed's shadow is cast by its blocks, the 2003 one by the artist, so it
matches less; the T-34's at step 2 falls on the road where step 0's lay on grass, and the threshold cuts the two
differently. The shed at step k is its `_r<k>_l<k>` sprite (IoU 1.000, colour 0.00 to 0.01) and not its `_r<k>`
(colour 27.2, 33.4, 14.0). The tree is its `1_l<k>` picture (IoU 0.976, colour 0.00 to 2.93; the 2003 picture 16.5 to
18.7), and the crown's left half against its right goes from 1.449 at step 0 to 0.705 at step 2 (0.851 and 1.041 at
the quarter turns). Remastered off: 0 pixels changed by the key.

`camera-turn-units-game-reads-it` holds the tanks the same way (it held them to the step-0 shadow at the turned place
before): turned / not turned 0.10 to 0.73 / 0.00 to 0.23 at step 1 (cast behind the tank: 59 to 149 pixels), 0.40 to
0.46 / 0.14 to 0.17 at step 2, 0.80 to 0.90 / 0.12 to 0.21 at step 3, centroids 0.6 to 16.9 degrees where held; a
tank's colour is reported only (0.01 to 0.15, above). `test-bk-views` gains a box drawn at the half turn under either
light (each pixel the grey lit as its face is under that light; the +y wall twice as bright under the camera's light,
the -x wall brighter under the world's sun) and a round crown relit for step 2 (its lit half moves from left to right).

### Other tiers (round 2)

Run on win-home (Windows x64, debug install), one `zig build` at a time, after the last engine change:
`camera-turn-sun-game-reads-it camera-turn-units-game-reads-it test-bk-views` pass;
`test-map-editor-engine map-editor-smoke camera-turn-world-game-reads-it camera-turn-game-reads-it` pass (628 s; the
shared `CameraTurn.h`, `MOBuilding` and `MOObject`, the Map Editor's step stays 0).
After the objects' relighting: `camera-turn-sun-game-reads-it camera-turn-world-game-reads-it objects-relight` pass
(1345 s; `objects-relight` notes the two mines on stderr and exits 0), then
`test-map-editor-engine map-editor-smoke` again (pass, 1100 s with other worktrees building).
The building batch (`building-views-batch-<region>`, now twice the turns) and the full sweep are left to the
maintainer; Linux and macOS to CI.

### Left open (round 2)

- **Infantry** keep their 2003 shadow and lit side at every step (above).
- **Mesh light:** the GPU renderer has no directional term, so a tank's lit side does not follow any sun; adding a
  Lambert term to the lit effect would light every mesh anew (2003's D3D fixed function did), a renderer change of its
  own.
- **Buildings turned in the Map Editor:** only `_r<k>_l<k>` (facing 0) is made; a building of facing f at step s
  needs `_r<f+s>_l<s>` and falls back to the light fixed to the camera with its step-0 shadow turned. `bk-views side`
  makes any `<k>l<s>`; the batch would grow from 6 to 15 turns a variant. The Resource Editor's Four sides still makes
  `1,2,3` only.
- **Objects' shadows** are the 2003 shadow sprites turned, which hold the shadow outside the 2003 picture only: where
  the object hid part of its shadow at step 0, that part is missing at the other steps (small under a tree or a box).
  Terra objects keep their painted relief as before (S03).
- **Relit objects are not gated by a style measure** (`bk-views style` compares buildings with their family's
  pictures); a crown seen at step 2 is the shaded side, about 0.6 of the 2003 brightness, as a building's back is.
