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
