# The Game camera turns in four steps (Remastered)

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
