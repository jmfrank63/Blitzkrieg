# Building views for a rotatable camera (Remastered)

Blitzkrieg draws every building, bridge, fence and most objects as a sprite rendered once from the
game's fixed camera. Vehicles and guns are 3D models (`Data/Units/Technics`: 500 `.mod`) and turn
freely; infantry has 8 facings. A camera that turns in 4 steps (N, E, S, W) therefore needs the
sides of every sprite object that were never drawn. Four, not eight (decided 2026-10-26, Johannes): a
quarter turn maps the diamond grid of AI tiles onto itself, so footprints, passability, terrain, roads,
fences, bridges, flora and objects turn with it unchanged; 45 degrees would need re-sampled footprints
and diagonal terrain, roads, fences, bridges, flora and objects. Eight stays possible later as more
folders beside the four. This project fills them, so that the Remastered
option ([remastered option](2026-10-25-remastered-option-design.md)) can offer the rotatable camera.
The original game (Remastered off) is not touched.

## Decisions (2026-10-25, Johannes)

- **Only Blitzkrieg 1 pixels.** Every pixel of a new side comes from the 2003 sprites, so the game keeps
  one style. Blitzkrieg 2 textures and models are never copied into the game.
- **Blitzkrieg 2 as a blueprint.** The published Blitzkrieg 2 data (`nival/Blitzkrieg-2`, non-commercial
  licence that allows study; also installed from Steam on win-home) may tell us what a hidden side looks
  like: the building's shape (towers, wings, apse, chimneys, roof) and where windows, doors, signs and wall
  materials sit. Those places are filled with Blitzkrieg 1 pixels.
- **Generative AI is allowed** to fill faces and to add variants, as long as the 2003 style is met: it is
  trained on Blitzkrieg 1 sprites only, guided by the building's geometry and its neighbouring 2003 pixels,
  and every result is checked against the 2003 sprites by code.
- **All new code in Zig.** Blender (only scriptable in Python) gets a thin script that reads a job file a
  Zig tool writes; ComfyUI is driven over its HTTP API from Zig.
- **No new hardware.** win-home: Blender 5.1, RTX 3060 Ti 8 GB, Xeon E5-2699 v3 (18 cores), 64 GB RAM,
  169 GB free on D:. Nothing in the pipeline needs more than 8 GB of VRAM (no TRELLIS).
- **Order:** the font engine and the languages are finished first, then the replay; this project runs in
  parallel on win-home.

## What exists (measured 2026-10-25)

- 155 building folders in `Data/Buildings/<region>/<season>/`, 116 building families. Suffixes
  `_1`, `_2`, `_3`, `_4` are the same building seen from different sides (verified by reassembling the
  sprites of `e_ctownhouse01_1/_2/_4`, `e_house01_1/_3`); `_1_2` is the same side with other textures.
- Side views per family: 8 families with 3 sides (`a_h07`, `e_ctownhouse01`, `e_house01`, `e_house02`,
  `e_house03`, `e_house11`, `e_shed02`, `townhouse05`), 20 with 2, 17 with one numbered side, 71 with a
  single sprite (churches, factories, stations, ...), none with all 4. About 150 of the 464 views a
  4-view camera needs exist.
- Variants of one building do not always share a colourway (`e_ctownhouse02_1` slate roof, `_4` tile roof).
- Each variant has its own footprint and passability (`1.xml`); damage states are `1.san`, `2.san`, `3.san`
  with shadows `1s.san` and ground parts `2g.san`, textures `<n>_c.dds` (colour), `_h`, `_l`.
- `.san` layout (confirmed in slice 1, see below): square texture pieces of 16 to 256 px, each 9 floats
  `x, y, size, u1, v1, u2, v2, depth_left, depth_right`; the texture `1_c.dds` is an atlas of those pieces.
- A building `.san` is not the animation format `bk-sprite compose` writes (`SSpriteAnimationFormat`): it is
  `SSpritesPack` (`Sources/src/Formats/fmtSprite.h/.cpp`), an engine structure file with the signature
  `0x314e4153` in chunk 127 and the pack in chunk 1: per sprite its squares (`vLeftTop`, `fSize`, `rcMaps`,
  `fDepthLeft`, `fDepthRight`), its edge (`CVarArray2D<short, BYTE>` with a bound box) and its `center`. The
  builder is `CSpritesPackBuilder::Pack` (`Sources/src/RandomMapGen/IB_StaticMethods.cpp`), reached from
  `NCompose::ComposeSingleObjectPack` (`Sources/src/ResourceModel/compose.cpp`), which the Resource Editor's
  building export calls with the picture's zero cross and the passability grid of `1.xml`.
- Blitzkrieg 2: 135 building models in Maya sources (`Complete/Buildings/<region>/<name>/*.mb`, not
  readable by open tools) and compiled geometry (`Versions/Current/Data/bin/Geometries/<uid>`, named by each
  building's `summer_<n>_geometry.xdb`), with damaged and destroyed states. 32 of our 116 family names match a
  Blitzkrieg 2 model exactly; more match under another spelling (`barracks`/`barraks`, `admhouse`/`admbuilding`).

## Pipeline

1. **Decompose** (`bk-views decompose`, Zig): a `.san` plus its `.dds` become a PNG per animation frame and
   damage state, with the screen anchor, the depth values and the footprint from `1.xml`. Changed 2026-10-25
   (Johannes): `bk-sprite` is C++ on the engine libraries, so decompose is a new std-only Zig tool, `bk-views`,
   that reads the structure file, `SSpritesPack` and `_h.dds` itself; a C++ test checks its reading against the
   engine's `SSpritesPack::Load`. The later Zig steps (camera, symmetry, fill, style check) join `bk-views`.
2. **Camera** (Zig): solve the 2003 camera (projection, tilt, pixels per tile) from the buildings that have
   2-3 sides, by fitting a box model's outline to their sprites. Stored once, used by every render.
3. **Symmetry** (Zig): compare each sprite with its mirror image; where the building is symmetric about the
   viewing axis, the opposite side is the mirrored sprite.
4. **Shape** (Zig, Blender to correct): a block-out mesh (walls, roof, towers, chimneys) as glTF, from the
   sprite outlines and the footprint; where a Blitzkrieg 2 model exists and its outline agrees with our sides,
   its shape is used instead (the converter reads the compiled geometry, using Blitzkrieg 2's loader source as
   the format specification). If the outlines disagree, our sprites win.
5. **Project** (Blender): every existing 2003 side is projected onto the shape from the solved camera, so
   every face an original sprite shows keeps its original pixels.
6. **Blueprint** (Zig, optional per building): from a matching Blitzkrieg 2 model, a label map of the hidden
   faces (wall, window, door, sign, timber frame, roof, plaster/brick) - which kind of thing sits where, no
   Blitzkrieg 2 pixels.
7. **Fill** the faces no sprite shows:
   - patch-based fill from the same building's own pixels (PatchMatch-style, in Zig), guided by the label map
     when there is one: windows from its windows, doors from its doors, wall from its wall;
   - generative inpainting where patches are too repetitive or a face needs something the building has no
     example of: ComfyUI with an inpainting model, ControlNet (depth, outlines, label map) and a LoRA trained on
     Blitzkrieg 1 building sprites per region (OneTrainer or kohya_ss; SD 1.5 fits 8 GB, SDXL is tight).
8. **Style check** (Zig): each filled face against the building's own 2003 sides - colour statistics, texture
   statistics, edge sharpness, light direction; a face that drifts is rejected.
9. **Render** (Blender): 4 views per building, damage states, shadow layers, seasons, at the solved
   camera, lit from the same screen direction as the 2003 sprites.
10. **Compose** (`bk-sprite compose-pack`, new, C++): renders back into `.san`/`.dds` variants beside the
    originals through the existing `NCompose::ComposeSingleObjectPack`; each orientation keeps (or gets,
    rotated) its footprint data. `bk-sprite compose` stays the animation composer it is.

## Slices

1. **Decompose and camera:** `bk-views decompose` with a round-trip test (decompose, `bk-sprite compose-pack`,
   decompose again, byte-compare the frames), and the camera fit on the 3-side families, measured by outline
   overlap.
2. **First building, end to end:** `e_ctownhouse01` (3 sides): shape, projection, the missing fourth side by
   patch fill, render, compose, and the Game drawing all four; measured against the three originals.
3. **Blueprint:** the Blitzkrieg 2 geometry converter and label maps, on `church` or another exact match;
   the outline check against our sprites.
4. **Generative fill:** the Blitzkrieg 1 LoRA, ComfyUI driven from Zig, the style check; compared with the
   patch fill on the slice 2 and 3 buildings.
5. **Batch:** all 116 families, a report per family (source of each face, style-check values), hand fixes
   for the landmarks the tools cannot do.
6. **Bridges, fences and objects:** the same pipeline for `Data/Bridges` (104 `.san`), `Data/Fences` (42),
   `Data/Objects` (1,229).
7. **The camera in the Game** (Remastered only): 4 rotation steps (N, E, S, W; why not 8: see the top), draw order per angle, picking, scrolling,
   minimap, sprite variant per angle (object orientation plus camera step), save and replay.

## Verification

- Every tool has unit tests; passing tests print nothing.
- A view counts as done only when the Game draws it: a capture tier per slice, measured by code against the
  2003 sides (outline overlap, style-check values), never judged by eye alone.
- The 2003 sprites are never modified; new variants are added beside them, and with Remastered off the Game
  loads none of them.

## Slice 1 results (2026-10-25)

Plan: [building views 01](../plans/2026-10-25-building-views-01-decompose-camera.md). Tiers: `test-bk-views`,
`bk-views-roundtrip`, `test-bk-sprite`, `test-bk-tools`, `test-bk-tools-l10n`, `test-bk-l10n`, `test-bk-image`, all
passing on win-home (Windows x64); Linux and macOS are left to CI.

- **`.san` layout.** An engine structure file (`StreamIO/StructureSaver2.cpp`): a chunk is an id byte and a length
  `n << 1` in one byte, or `(n << 1) | 1` in four bytes when that is 256 or more, then `n` bytes. The file holds
  chunk 1 (and empty chunks 0 and 2 for objects); inside it chunk 127 is the signature `u32 0x314e4153` and chunk
  1 the pack. The pack's chunk 1 is the sprite vector, one chunk 1 per sprite; a sprite is chunk 1 squares
  (chunk 1 the count `i32`, chunk 2 the raw 36-byte squares), chunk 2 edge (chunk 1 `bool` horizontal, chunk 2
  the `CVarArray2D<short, BYTE>` as chunks 1 size, 2 line count, 3 raw `i16` crossings, 4 raw `u8` line sizes,
  chunk 3 the bound box as 4 `f32`) and chunk 3 the center (2 `i32`). Every building pack has one sprite. The
  center is the zero cross in the 2003 source picture; squares are relative to it, on a grid of their own size.
- **Decompose.** `bk-views decompose` (std-only Zig) writes what `CSpritesPackBuilder::Unpack` makes, on one
  canvas per variant that starts at the 2003 picture's corner. `bk-views-check` reads the same packs with the
  engine's structure saver and `Unpack`, and every field and pixel agrees.
- **Round trip.** `bk-sprite compose-pack` (`NCompose::ComposeSingleObjectPack` and `ComposeNoisePack`, the
  Resource Editor's building composer) on the decomposed pictures, with the zero cross, the passability of
  `1.xml` and the grid origin on screen under the MFC camera, gives back every `.san` and `_h.dds` of
  `e_ctownhouse01_1/_2/_4` and `e_house01_1/_2/_3` (sprites, shadows and ground pictures of the three damage
  states) byte for byte. So the pipeline's last step reproduces the 2003 export exactly, and a new view
  composed the same way is a pack the Game reads as it reads the originals. `_c.dds` and `_l.dds` are
  re-encoded and only within the DXT tolerance of the Resource Editor port; the Game draws `_c`/`_l`.
- **Camera.** The 2003 camera is a pure orthographic projection, and it is `MfcEditorCamera`: the 928 picture
  and world point pairs of the 25 variants of the eight three-side families (fire slots, fire, smoke and
  explosion points in `1.xml`) fit one linear camera with an rms of 0.0002 px: one pixel per world unit, yaw
  45 degrees, pitch -120 degrees (30 degrees above the ground), an AI tile (`16 * sqrt(2)` world units) 32 x 16
  px. A box over each locked footprint, its height fitted, overlaps the sprite outline with IoU 0.69 to 0.95
  (pitched roofs, chimneys and porches are what a box misses). Recorded in
  `tools/zig/fixtures/bk-views/camera-2003.json`; `test-bk-views` fails if the fit leaves it or an IoU drops by
  more than 0.01.
- **Seen on the way.** `NResourceModel::DefaultEditorCamera()`, the exporters' fallback without an engine
  camera, has half this scale (its `m11` is 0.354 where the shipped art's is 0.707); exports through the bridge
  and the golden comparison use the engine or MFC camera and are not affected.

## Inventory and side convention (M005 S01, 2026-10-25)

`bk-views inventory Data/Buildings <out>` writes
[the inventory](../evidence/building-views/inventory.md) and `tools/zig/fixtures/bk-views/inventory.json`:
115 families (variant folders grouped by name without the side suffix; numbered subfolders such as `dot/01`
are variants of their folder's family), 194 variant folders, each with its measured side, missing sides,
colourways and winter packs. `test-bk-views` fails when the tracked files differ from what the tool writes.

- **Convention.** Side k + 1 is side k turned by the quarter turn that takes world (x, y) to (-y, x) about the
  zero cross: the footprint's front corner goes to the right corner, the left wall becomes the right wall, and
  every 1.xml `Direction` grows by 90 (Direction 0 faces +y, 90 -x, 180 -y, 270 +x). `turnPoint`, `turnGrid`
  and `turnXml` turn a variant's passability and visibility grids with their origins, every `Position` and
  `WorldPosition`, every `Direction`, and set each `PicturePosition` to the 2003 camera's projection of the
  turned `WorldPosition`. A variant without a suffix starts at side 1.
- **Measured, not assumed.** The suffix holds on every variant of `a_h07` (1, 2, 3), `e_ctownhouse01` (1, 2, 4)
  and `e_house11` (1, 3, 4), and on most two-variant families. It does not hold everywhere: several variants
  repeat an earlier variant's view with other textures or details (`e_house01_3` = view of `_1`, `e_house02_2`
  = `_1`, `e_house03_4` = `_3`, `e_shed02_5` = `_2`, `townhouse05_3` = `_2`, both `_1_2` colourways), and a
  few measure as another side or cannot be measured (listed at the end of the inventory). So the spec's
  "`e_house01_1/_3` are different sides" above is wrong: they show the same side of two differently roofed
  houses. The batch therefore takes a variant's side from the inventory, not from its suffix.
- **How.** Same view: both visible walls correlate (brightness, sheared level, quarter scale) at 0.5 or more and
  the outlines overlap 0.75 or more. Otherwise the footprints (passability IoU under the four turns) decide
  even or odd, or the outlines when the footprints fit both ways; even is a half turn; odd is the better
  footprint, else the shared wall. The passability grids are hand-painted and the colourways differ, so
  the odd-turn direction is the weakest step: by eye `e_jettybuilding_4` (measured 2) and `e_townhouse02_3`
  (measured 1) look like their suffix sides 4 and 3. `dot` and `dot_h` hold the same pictures with different
  grids; `dot_h` measures as sides 1-4, `dot` does not.

## First generated side (M005 S02, 2026-10-26)

`bk-views side <variant-dir> <turns> <out-dir>` writes the variant turned by each of `<turns>` (1, 2, 3 quarter
turns) as `<out-dir>/<variant>_r<k>/`, a folder like a shipped variant (packs of every damage state with shadow and
ground pictures, winter packs where the variant has them, the turned `1.xml`, `icon.tga`, `name.txt`), and
`<variant>_r<k>.map/` with the source of every pixel (`sources.json` and a map per picture). Output is per variant,
not per family (decided by the milestone's orchestrator): two variants that share a view each get their own three
turns, and colourways stay apart. `zig build bk-views-side` makes `e_ctownhouse01_2_r1` (side 3, which no 2003
picture shows), `_r2` (side 4) and `_r3` (side 1) and checks every pack like the round trip (the engine's reader,
compose-pack again byte for byte, decompose back). The voxel hull, Blender's rays and the mirrors below were replaced
in S03 (next section).

- **Shape.** A voxel visual hull (2 world units) in the variant's frame: every voxel over its footprint (grown by a
  cell) whose centre every picture of the family covers, the other variants' zero crosses found by maximising the
  hull's volume. From 30 degrees up a wall facing the eye hides what stands in front of it except at its foot, so
  a column stands only on a kept ground voxel. Hull outline against the 2003 outlines (IoU): 0.96 to 0.98 intact,
  0.91 to 0.94 damaged, 0.82 to 0.93 ruined. Every variant of the family gives the shape; only those of the
  target's colourway give pixels (hue histograms differ by less than 0.1: 0.014 for `e_ctownhouse01_2`/`_4`,
  0.16 for `_1`).
- **Blender casts the rays.** `tools/blender/bk_views_render.py` builds a BVH of the hull's faces and writes, per
  pixel of each view, the hit point and which 2003 eye sees it (directly or through a mirror plane of the hull), and
  whether the ground there lies in the hull's shadow. No Blender renderer is involved, so the output is exact and
  byte-identical run to run (and between Debug and ReleaseFast bk-views). Everything else is Zig.
- **Pixels.** A seen pixel takes the 2003 picture that sees it, relit by the ratio of mean colours of its shading
  class (which way it faces in the camera's frame) between the target and the source; then mirrored pixels (mirror
  planes whose hull IoU is 0.8 or more: `x = y` at 0.85 here); the rest is patch-filled (PatchMatch-style
  propagation and random search, 7x7 patches, the same shading class, fixed seed). The light is fitted to the 2003
  shadow pictures (azimuth 245, elevation 67.5 in the camera's frame, shadow IoU 0.73); shadows are the hull's,
  cast by Blender. Ground pictures are the 2003 ones laid on the ground plane.
- **Icon.** `compose-pack --icon` runs the export's `SaveIconFile`; on `e_ctownhouse01_2`'s decomposed picture it
  gives the shipped `icon.tga` but for one byte off by one.

## Quality gate and the block model (M005 S03, 2026-10-26)

The S02 side read as a lumpy, smeared mass: the hull carved from 30-degree outlines got the roofs wrong, and texture
projected onto a wrong surface smears. S03 measured that first and then replaced the shape, the ray casting and the
mirrors (the S02 section above describes what was replaced).

- **Gate.** `bk-views holdout <out-dir> <variant-dir>...` draws a side that exists in 2003 again from its family
  without it (and without any variant showing that side), from another side of its colourway, and scores every damage
  state against the 2003 picture: outline IoU, SSIM of the brightness at a quarter of the size (the layout; at full
  size every drawn side scores near 0), sharpness (gradient energy ratio), mean colour and hue distance.
  `bk-views style <out-dir> <variant-dir> <pictures-dir>...` scores pictures that have no 2003 original against the
  building's own 2003 sides in its colourway. 23 variants of 11 families qualify (two or more measured sides, another
  side of the colourway by the hue rule). [The hold-out](../evidence/building-views/holdout.md) and
  [the style of e_ctownhouse01's sides](../evidence/building-views/style.md) are recorded;
  `tools/zig/fixtures/bk-views/gate.json` holds floors from them, and `test-bk-views` draws the 23 again (with a
  ReleaseFast host build) and fails below them.
- **Blocks.** The building is a few blocks: the rectangles of the hull's ground slice within half a cell of the
  footprint, each with a flat, gabled or hipped roof. Each block takes its best roof and heights on an 8-unit grid,
  then every edge and height moves while the outline IoU over the family's pictures improves, less 0.02 IoU per volume
  of the start (one outline leaves the depth of what stands in front open, and without that cost blocks grow into
  towers seen from behind); roofs are no steeper than 2.5. Each damage state is fitted from the intact blocks to its
  own outlines; the ruins too (their hold-out IoU rose from 0.59 to 0.74, so the voxel path was not kept).
- **Rays in Zig, Blender gone.** A block is the intersection of at most nine half-spaces, so a ray of the exact
  orthographic camera meets it in closed form. `tools/blender/bk_views_render.py` and the job file are deleted; `side`
  takes 14.5 s for three turns of e_ctownhouse01_2 in ReleaseFast (S02: 26.6 s) and stays byte-deterministic (also
  between Debug and ReleaseFast).
- **Faces nobody sees.** A point of a face no 2003 picture of the colourway sees borrows from the first of the face's
  donors one sees: its block's opposite face first (the half turn, so writing stays readable), then faces of other
  blocks alike in width and height, facing the same way, the opposite way or across; the point keeps its place across
  the face, and its height is scaled so that eaves and ridges meet. PatchMatch fills only what is left.
- **Light.** One constant: azimuth 245, elevation 67.5 in the camera's frame. Fitted to the 2003 shadows of
  e_ctownhouse01, a_h07, e_house01, e_house11 and townhouse05 with the hull it came out at 232 to 258 and 67.5 to 75,
  the spread of the hull's shape.
- **Measured.** Means of state 1 over the 23 variants, S02 to S03: outline IoU 0.672 to 0.860, colour distance 33.8
  to 18.6, hue 0.242 to 0.123, SSIM 0.154 to 0.170; sharpness 0.90 to 1.30 (from smeared to more edges than the 2003
  sides: seams where a face changes source, chimneys painted on roof planes). States 2 and 3 likewise for IoU, colour
  and hue, with SSIM a little lower.
- **Not done.** Chimneys, spires and dormers are not modelled: the hull's cap above the roofs merges with them, so
  they could not be cut out of it; their pixels land on the roof planes. The colourway rule (whole-picture hue
  histograms) is strict and leaves 70 of the 93 measured multi-side variants out of the hold-out.

## The Game draws it (M005 S04, 2026-10-27)

- **Tier.** `zig build building-views-game-reads-it` (`tools/zig/building_views_game_reads_it.zig`) writes side 3 of
  e_ctownhouse01 (`e_ctownhouse01_2_r1`) and sides 2 to 4 of the single-sprite e_frenchshed01 with `bk-views side`
  straight into a scratch mod's `data/buildings/views/`; the mod's `modobjects.xml` registers them as buildings
  (`Views_<variant>`, read by `CObjectsDB::LoadDB` after objects.xml). The 2003 sides keep their objects.xml names.
  Four maps from `river3d.xml` each hold side k of both families, 500 pixels apart: the map is too small for four
  town houses in one frame. The Game runs from `zig-out/local-test/building-views/game` (S06: its own folder, so
  it no longer deletes the batch's work beside it) with every user path redirected
  and `BK_SCENE_DISABLE` turning every pass but the objects off, so only the buildings and the interface are not
  black.
- **Placement, measured.** `camera=` takes the AI's coordinates (visual over sqrt 2): camera 512 puts a building
  at the visual (724, 724) on the screen's (511, 383); a visual step of (1, 1) is 1 pixel right, (1, -1) a quarter
  pixel down.
- **Measured.** Each building against its decomposed intact sprite (the `_h.dds`), zero cross where it stands, best
  offset within 4 pixels: outline IoU 0.994 to 1.000 at offset 0,0, mean colour difference 0.00 for all eight sides
  (the Game draws the pack's pixels unchanged); the best IoU of another side of the family at the same place is
  0.654. Floors: IoU 0.95, colour 2, and the own sprite must beat every other side. With the side-3 maps given the
  wrong pictures (side 2), the tier failed at IoU 0.166 and 0.133.

## Batch over all variants (M005 S05, 2026-10-26)

- **Unit and layout.** Every one of the 194 variants gets its three other quarter turns (`<variant>_r1.._r3`, per
  variant rather than per family side, because two variants of a family can show the same side in other
  colourways): `zig-out/remastered/Buildings/<region>/summer/<variant>_r<k>/`, a numbered subfolder `dot/01`
  giving `dot/01_r1` beside it. A folder holds what the shipped variant holds, winter packs included, except the
  stray texture-less `2gw`/`3gw` of `townhouse03_1`; where the 2003 variant has no shadow pack for a state
  (pillboxes, tracks, some ruins: 22 variants) the turned one has none either. The `.map` and `.work` folders go
  to `zig-out/local-test/building-views/batch/<variant path>/` (`bk-views side`'s optional `<work-dir>`).
- **Steps.** `zig build building-views-batch-<region>` (africa, common, europe, ussr): per variant `side`, the
  round trip runner (`bk-views-check`, compose-pack again byte for byte, decompose back), `style` on the generated
  pictures and `holdout` where the family has a second measured side. Measured on win-home from a clean output:
  38 s, 48 s, 121 s and 181 s; two runs give the same bytes. The runs are silent and judged by exit code: a run
  that inherits stderr holds the build's stderr lock, and one that pipes stdout and stderr goes through one
  multi-reader; either way the variants ran one at a time on Windows (about 5 minutes for africa instead of 40 s).
  `building-views-batch` only lists the groups; `building-views-batch-report` writes `batch.md` and
  `<family>.md` under `docs/superpowers/evidence/building-views/` with `bk-views report`.
- **Fixes the batch needed.** A numbered subfolder's family was found one level too high (each subfolder its own
  family); a variant whose side the inventory could not measure (20) is drawn from itself alone; an object too thin
  for a hull rectangle (conduitbarrel, watchtower) starts from one block over its footprint; a ruin whose shadow
  falls all under it keeps one shadow pixel below it (an empty shadow pack does not compose).
- **Sources.** The source map tells mirrored (a face of the same block: the opposite wall, the other roof plane)
  from transferred (a face of another block).
- **Result.** 582 of 582 folders, every pack through the engine check; 168 of 194 variants pass the S03 floors
  (hold-out 35 of 43); 59 variants flagged for S06 by the shape: turned sprite height outside 0.7..1.3 of the 2003
  one (20, e.g. the lost chimney stalks), blocks' ground outside 0.5..2 of the footprint (20, mostly open props),
  intact outline IoU under 0.85 (34).

## Fallbacks, chimneys and the shape gate (M005 S06, 2026-10-26)

S05's batch passed 168 of 194 variants, but the gate looked at style and hold-out only: `e_house02_1_r2` passed with
roof peaks no 2003 side shows, and `conduitbarrel` (a standpipe) became a grey block. S06 measured the shape, made the
generator prefer fewer, lower blocks, cut chimneys from the outline, turned thin props as their own 2003 sprites, and
stopped taking pixels from faces seen at a grazing angle.

- **Contact sheets.** `bk-views contact <buildings-dir> <generated-dir> <out-dir>` draws every variant a row, its 2003
  intact sprite (on blue) and its turns `_r1.._r3` (on grey) at half size, labelled, a family under a yellow line, on
  pages `contact-<n>.png`. The S05 output is in `zig-out/local-test/building-views/contact-before-*.png`, the S06
  output in `contact-*.png` (not tracked). The source map folder of each turn also holds `<state>_shape.png`, the
  blocks lit and tinted per block.
- **Fewer, lower blocks.** Each block costs 0.03 of the pooled outline IoU: after the descent, the block whose loss
  costs least is dropped while that is less than its cost, and the rest refit (S05's stray towers were chimneys, its
  fins gable ends). A block is at least 12 world units each way (a thinner slab shows as a fin turned), its roof no
  steeper than 1.75 (S05: 2.5; a steep roof on a narrow block is a spike from behind), and, but for a ruin's rubble,
  its roof stands on walls a third of its height (a roof down to the ground stood in for a facade). Hold-out (the 23
  variants, [the record](../evidence/building-views/holdout.md)), S03 -> S06: state 1 IoU 0.860 -> 0.868 (worst
  0.693 -> 0.715), SSIM 0.170 -> 0.190, colour 18.6 -> 16.4, hue 0.123 -> 0.101; states 2 and 3 alike, the worst
  ruin IoU 0.658 -> 0.613 (floor 0.6). The wall rule for ruins took e_shed01_2's ruin hue to 0.27, over its floor,
  so ruins are exempt. A volume cost of 0.005 instead of 0.02 joined some single-view wings but took the worst ruin
  IoU to 0.586: not kept.
- **Chimneys, towers and spires** (`cutChimneys`). The outline of the variant's own picture above the blocks' outline
  falls into parts; a part 3 to 48 pixels wide at its foot (the widest row of its lower half) and 5 or more high is
  a flat-topped square block as wide as that foot seen corner on, up to where it narrows under 0.7 of it, and a
  pyramid on it up to its top. It stands where its foot meets the top of the roof's outline (a ridge), unless along
  the eye's line of sight, where it shows the same, the family's other pictures hold more of it. Every state but the
  intact one is fitted from the intact blocks without them and gets its own. Test: a 6-unit chimney on a gabled
  house comes back within 4 units of its place and 3 of its height and width.
- **Grazing faces.** A face a 2003 picture sees at an angle whose cosine is under 0.25 is taken from a picture or a
  donor face seen squarely first (factoryfloor01's turned roof was stripes smeared from a sliver seen edge on).
  Seams, measured as the style sharpness of the 864 generated summer and winter intact pictures: mean 1.175 -> 1.137,
  pictures over 1.3 187 -> 155; transferred pixels 7.5% -> 3.6% of the intact sprites, patch-filled 0.8% -> 0.5%.
  One source picture per face (each face first from the source that gives it the most pixels) was tried: state 1
  SSIM 0.195 -> 0.173 and roofs relit too bright (e_frenchshed06): not kept.
- **Shape gate** (`shapeFailure`, every turn drawn by the blocks): the turned intact sprite's height 0.7..1.3 times
  the 2003 one's; the blocks' ground 0.5..2 times the footprint's cells; the blocks' outline IoU 0.85 or more against
  the variant's own 2003 picture (other variants of a family may be other buildings: e_house01_1 and _3 differ in their
  roofs); and the **protrusion**, what an opening of the top edge 48 pixels wide takes off the outline as a share of
  its area, at most 1.5 times the 2003 sprite's plus 0.03. Between the 2003 sides of one building the protrusion is at
  most 1.5 times plus 0.026 (a_h07_3 against _1; tents and busts aside); S05's e_house02_1_r2 had 0.095 against the
  2003 0.028 (limit 0.072) and fails. Outline-only measures were tried first and dropped: the turned outline against
  the 2003 sides' (mirrored for an odd turn) and peak counts of the top edge do not separate e_house02_1_r2 from the
  spread of real 2003 sides (its family's own sides differ by 0.12 to 0.15 in top-edge height), and counting the
  blocks' pieces failed sound sheds (e_frenchshed01's annex stands 12 units off) while towncornhouse04's pieces stand
  6 to 9 units apart. Under these measures S05's output would pass about 102 of 194 (its flags as failures, its
  protrusions measured on its sprites); S06's passes 142.
- **2003 sprite fallback** (`sameSprite`). A thin object (its 2003 protrusion 0.15 or more: stalks, tanks on legs,
  statues, wells, fountains, hydrants, which blocks of 12 units cannot draw; houses stay under 0.1) or a small one
  (4 footprint cells or fewer) whose blocks fail the shape is turned as its 2003 pictures themselves, every state,
  shadow and ground picture, moved so that it stands on its turned footprint, where the turn keeps the footprint
  (cell IoU 0.9 or more): 40 variants, e.g. conduitbarrel, the chimney stalks, the water tanks and tower, lenin, the
  statues' half turns. A mirrored sprite for the odd turns of two-fold props (kiosks, dustbins, cisterns, leaders)
  was not built: it would light them from the wrong side; those turns stay on blocks or on the hand-fix list.
- **Blueprints and generative fill: not built.** After the steps above the failures are shape (38 variants by the
  shape gate, 18 more by eye) and not texture: 14 variants fail on style or the hold-out, and
  patch fill draws 0.5% of the turned intact pixels. Blitzkrieg 2 blueprint labels place windows and doors and
  generative fill paints faces; neither addresses a wrong block layout. A Blitzkrieg 2 geometry converter (32 exact
  family matches) is what the multi-wing hand-fix cases would need; it is left for a later slice.
- **Result** ([batch](../evidence/building-views/batch.md)): 582 of 582 folders, every pack through the engine check;
  124 of 194 variants pass the gate (style, hold-out, shape, and not listed by eye); hold-out 34 of 43; the rest are
  listed in batch.md with their first reason, for hand fixing: 38 by the shape gate (outline 12, ground 11 - open props
  such as tents and track ends -, protrusion 8, height 7), 14 by style or the hold-out (winter hue of the pillboxes,
  sharpness of ruins), and 18 that pass the measures but look wrong on the contact sheet, listed with the reason in
  `gate.json`'s `by_eye` (multi-wing town houses split or jumbled: e_ctownhouse02/03/04, towncornhouse02/04; single-view
  depth: a_wb; flat roofs grown pitched: a_h04_1; tanks lost: a_cisterns; and church, factoryboiler, house07,
  housebig, e_house09_1).

## Hand shapes (M005 S07, 2026-10-26)

S06 left 70 variants for hand fixing (38 by the shape gate, 14 by style or the hold-out, 18 by eye). S07 draws the
buildings the fit cannot model from hand-authored blocks and fixes the style and hold-out failures at their cause.

- **Hand shapes.** `tools/zig/fixtures/bk-views/shapes/<family>.json` maps a variant folder's name to its intact
  blocks in its own frame (world units from its zero cross; the block format of sources.json: rectangle, wall,
  ridge, roof flat / gable_x / gable_y / hip; a spire is a narrow hip block). `side` and `holdout` take them instead
  of the fitted intact blocks, in summer and winter and for a damage state whose own picture they still cover
  (outline IoU 0.85); the other damage states are fitted from them. No chimneys are cut from a hand shape: its author
  draws them. 54 variants of 38 families have one: the multi-wing town houses (e_ctownhouse02/03/04,
  towncornhouse02/04), the stations and city hall, church, factoryboiler, e_house09_1, house04/06/07/08, housebig,
  a_h01, a_h04_1, a_h09_2, a_mosque_01_4, a_wb, a_cisterns, and open props (tents, the wt tarpaulin stacks, track
  ends, pillboxes, cistern/02, pigeonloft/01, e_statue/04).
- **Authoring.** `bk-views shape <out-dir> <variant-dir>...` writes, per variant, the outline IoU of its intact
  blocks (hand shape or fit) against every 2003 picture of the family, an overlay per picture, the blocks seen from
  the four sides and the blocks refined to the outlines with their roofs and pitches kept (edges, whole moves, up
  and down), and shape.json. Shapes were started from the footprint grid and the 2003 sides by eye, then refined by
  that tool or by a scratch maximin search over its IoU; the tracked numbers are the final blocks.
- **Test.** `test-bk-views` runs `shape` on every hand shape: its own 2003 picture reaches the shape gate's outline
  floor (0.85), the family's other pictures of its colourway the hold-out's mean floor of the intact state (0.84;
  their zero crosses are aligned to about a world unit and sides read from hand-painted grids). Other colourways are
  not held to it (e_shed02_4 is an open shed, _5 a closed one).
- **Shape gate for hand shapes.** The height and ground ranges guard the fit's depth, which one picture leaves open;
  a hand shape answers to its outline and protrusion only. For every turn the height is now taken against the 2003
  picture of the side it shows where the family has one (an L-shaped house turned stands higher or lower).
- **Mirrored 2003 sprite.** An odd turn of a thin object, or of a small one whose blocks fail, on a rectangular
  footprint is its 2003 pictures mirrored across the screen's vertical, which lays it on the turned footprint;
  the mirrored sprite is relit by the per-class gain from the face each pixel shows to the face its mirror shows,
  pooled over the sprite (per pixel it took the seams of the blocks: cistern/02 sharpness 3.0). 15 variants
  (tents, kiosks, dustbins, cisterns, leaders, statues, track ends). The hold-out draws a turn the way `side` does,
  the fallback included.
- **Style fixes.** The hue distance compares hue bands as shares of every opaque pixel, not of the coloured ones: a
  snowed pillbox has few coloured pixels, whose hue alone swung by up to 1.25 at a colour distance of 10 (dot,
  dot_h winter). The relighting gain is not applied where the pictures show the target class on a quarter of the
  source class's pixels or fewer (admhouse's porch roofs had relit its snowed main roof dark: winter colour 75.6).
  A damage state whose 2003 picture is the intact one (tarpaulin stacks, tanks) is judged by the intact floors.
- **Result** ([batch](../evidence/building-views/batch.md)): 190 of 194 variants pass (S06: 124), hold-out 40 of 43
  (34), 0 by eye (18); 582 of 582 folders through the engine check. Hold-out means, state 1/2/3: IoU 0.883 / 0.827 /
  0.761, SSIM 0.183 / 0.060 / 0.049. Left, with the measured reason: dzot/02 r1 ruin sharpness 0.67 and dot_h/02
  hold-out 3w sharpness 1.61, against the band 0.70..1.55 (a flat mound ruin, within 0.05 of the band); leaders/01
  and /02 hold-out (sharpness 0.44, colour 110): the board has two pictures, of its lit and its shaded face, and
  the hold-out, drawing one from the other, has no picture of the target face to relight from; the turns `side`
  makes from both pass style.

## S07's four left over, and a steady Game tier (M005 S08, 2026-10-26)

Each was fixed at its measured cause; no band of gate.json was widened.

- **dzot/02** (ruin sharpness 0.67). Its side is not measured, so `side` draws it from itself alone, but `style`
  judged it against every dzot picture of its colourway, whose ruins have 1.5 times the gradient energy of its own
  (2257, 2178, 2254 against 1481 per pixel). `style` now judges such a variant against itself alone, as it is drawn:
  0.91, 0.96, 0.97 for r1..r3.
- **dot_h/02** (hold-out 3w sharpness 1.61). The hold-out scored the drawn side against the held 2003 picture, but
  the 2003 sides differ by themselves: dot_h/01's snowed ruin, which it is drawn from, has 1.32 times the gradient
  energy of /02's, so a perfect copy scored 1.32. The hold-out's sharpness is now the drawing's against the
  picture it was drawn from, both at their own brightness (gradient energy over the squared mean brightness, since
  relighting a face scales its gradients): 0.97. Colour, hue, IoU and SSIM are still against the held picture.
- **leaders/01, /02** (hold-out sharpness 0.44, colour 110). Three causes. The fitted block of the board was a
  56 x 22 box with a gable roof, which put its one face on both walls; a hand shape (`shapes/leaders.json`, a slab
  4 units thick) fixes that, own outline IoU 0.89 and 0.93. The hull aligned the two boards 27 units apart along
  their length (where the hull's volume does not care; their 1.xml points put them 64 apart): with a hand shape the
  other pictures' zero crosses now take the shift whose outline the hand blocks cover best (`alignToBlocks`, from
  the hull's shift and from the footprints'), the other board's IoU 0.40 -> 0.91. And the hold-out has no picture of
  the target face to relight from: where the pictures show one wall the camera sees on a tenth of the other's
  pixels or fewer, the gain is the 2003 ratio of the lit wall to the shaded one, `wall_light` = 2.01 (the median of
  13 houses' intact pictures, spread 1.51..2.28; the boards measure 1.93; one factor, as the per-channel medians
  1.82, 2.03, 2.71 took the board's hue to 0.81). At a quarter, as for roofs, it also relit e_house11_3's ruin
  (3193 against 14664 pixels) and stubtrack/03 (87 against 381) over their floors, so a tenth. Hold-out now
  colour 37.1 / 18.9, sharpness 0.69 / 1.00.
- **Result** ([batch](../evidence/building-views/batch.md)): 194 of 194 variants pass, hold-out 43 of 43, 56 hand
  shapes; `test-bk-views` passes. `holdout.md` stays the S03/S06 record.
- **building-views-game-reads-it.** The flaky run had a whole shot about 50 pixels up. The schedule set the camera
  and parked the cursor one frame later; in that frame the cursor sat wherever the system's pointer left it, and on
  the window's edge (within 2 pixels) the mission scrolls the view. Shown on the Game: a cursor held on the bottom
  edge for a second after `camera=` scrolled both buildings out of the shot. The cursor is now parked first
  (`g1000:hover=1000x400,g10:camera=...`). Three runs in a row passed (IoU 0.994..1.000, colour 0.00).

## Four sides in the Resource Editor (M005 S08, 2026-10-26)

- **The layout the Game loads.** Sides 2 to 4 of a building variant are the sibling folders `<variant>_r1`,
  `_r2`, `_r3` that `bk-views side` writes: side k + 1 is side k turned by (x, y) -> (-y, x) about the zero cross
  (S01's convention), each a folder like the exported variant (the packs of every damage state with their shadow,
  ground and winter pictures, `icon.tga`, `name.txt`) with its 1.xml turned (every `Direction` + 90 per turn, the
  grids and points turned, the `PicturePosition`s projected again). No new format: a turn needs no objects.xml
  entry of its own, the Game's camera (plan item 7) finds a variant's turns by this path. The game tiers register
  them under names of their own only to place them on a map while the camera does not turn yet.
- **bk-views on a modder's folder.** `side` looks for a family only where Data's layout puts one
  (`<root>/<region>/<summer|winter>/<variant>`); any other variant folder, such as a mod's
  `data/buildings/<name>`, is a family of one, side 1, drawn from itself (before, the walk up from such a folder
  listed whatever lay three folders up as regions and seasons). A `shape.json` in the variant's folder, a list
  of blocks in the format of `tools/zig/fixtures/bk-views/shapes`, is its hand shape and comes before the tracked
  ones. Test (`test-bk-views`): a copy of e_frenchshed01 outside Data with a shape.json is drawn on that block.
- **The Sides window** (Building editor; `Sources/editor/resource_app/sides_dock.zig`, its logic in
  `sides_logic.zig`). The variant folder, filled in from the project's export (`<export dir>/data/buildings/
  <project folder>`) and editable; damage state 1 to 3, winter and shadow; the variant and its three turns side
  by side at one scale, each on a #2e3a6e ground, decoded from the packs on disk; a missing turn says so. The pack
  formats moved from bk_views.zig into `tools/zig/bk_views_pack.zig`, so the editor reads packs as bk-views does
  without the tools' language tables. **Generate sides** runs `bk-views side <variant> 1,2,3 <its folder> <work>`
  beside the editor as a child process the frame polls; the work folder `.<name>.views-work` beside the variant is
  deleted afterwards. **Import side picture** replaces the selected turn's picture of the shown state with a PNG
  (8-bit RGB or RGBA, decoded in the editor): its zero cross where that picture has it (the canvas `bk-views
  decompose` writes), laid on one canvas with the state's shadow and ground pictures as they are, and recomposed
  by `bk-sprite compose-pack` with that turn's footprint (the passability and origin of its turned 1.xml). A state
  without a shadow pack keeps none; a picture covering all its shadow keeps one shadow pixel below it, as `side`
  does (compose-pack refuses an empty shadow). bk-views and bk-sprite are installed beside ResourceEditor, where
  bk-sprite finds the engine libraries on every host (the game's `Tools/` folder does not serve it on Windows).
  Strings: `resource_app.window.sides` and `resource_app.sides.*` in the ten tables.
- **Tests.** `test-resource-app-logic`: the folders, pack names, a shipped pack read to its picture and footprint,
  both command lines, and PNG reading (the stored ones bk-views writes; a zlib PNG with all five row filters).
  `resource-editor-auto-bld` (frames 399 to 410): a scratch copy of ussr/summer/shed01 (one sprite, winter packs)
  shown with 3 of 4 turns missing, Generate sides, none missing, every cell's picture measured in the shot (2154 to
  2265 pixels that are not the ground, intact; 71 to 168 for the shadows of the winter ruin), then the tracked
  `side-import.png` imported as turn 2's damaged picture: 4800 pixels of its #10e0a0 in the shot.
  `building-views-game-reads-it`: the shed is now a modder's copy of e_frenchshed01 in the mod's own folder, its
  turns made with Generate sides' command line; the Game draws all four from the mod (IoU 0.994 to 1.000, colour
  0.00).
- **Not done.** The Map Editor (side = object orientation + camera step) belongs to the Game camera's slice. An
  imported PNG is not checked against the side's style; `bk-views style` can score it.

## Low parts and roof parts: ramps, steps, porches, vents (M006 S07b, 2026-10-27)

The maintainer's hand test: the barn `europe/summer/e_wb/01` (corrugated roof, three vents) shows two loading ramps
on its long side; its turn that shows that wall from the other side (`01_r1`) had none, nor had e_shed01_3's r1
(the same barn with a tiled roof). Cause: a block is at least 12 world units each way and costs 0.03 of the outline
IoU, so the fit never kept a ramp 13 units deep and 7 high; the 2003 pixels of the ramps had no surface to land on in
a turn. Hand shapes had none either.

- **Low parts** (`lowParts`). Per column of a 2003 picture, the lowest point the building's blocks cover is a wall's
  foot; where the outline runs on 3 or more rows below it (after up to 12 rows of ground, seen between the wall and a
  part's corner), a run of such columns under one wall is a flat block on the ground against that wall: as wide as
  the run's lowest pixels reach along the wall where they meet the ground (not round its corners), as deep as the
  median column reaches out (a ladder leaning on house07's wall reached 21 units in a few columns; the maximum made it
  a slab), half as high as deep (2 to 8 units), and only where 0.7 of what it adds to the outline is the picture's
  and it is 3 to 32 units deep (deeper is a wall the blocks missed). Every damage state, fitted or hand-shaped, takes
  the low parts of every picture of its colourway, each against the blocks without low parts: a ramp only a sibling
  shows (e_shed01_3's for e_shed01_2) stays on every turn. `shape` measures the blocks without them (the authored
  hand shape: with the pillbox's earth slabs counted, dot/03 against /02 fell below the floor 0.84, to 0.839).
- **Gate** (`lowKept`, in the shape gate): the low parts the 2003 intact pictures of the colourway show, seen from
  the turn's eye where they add to the outline of the blocks without them, must be 0.8 or more opaque in the turned
  intact picture wherever they cover more than 40 pixels. sources.json and the family reports carry both counts
  (`low`, `low_kept`). Tried first and dropped: the pixels below the walls on the picture's left of one side against
  those on the right of the next (a quarter turn mirrors a wall's foot). It holds for a box, but on L-shaped houses
  another wing hides the part in one of the two views: e_ctownhouse02_1_2 r3 measured 0 against 927 with the part
  drawn.
- **Measured** (`zig build building-views-batch-<region>`, then `bk-views report`). With the gate and the generator
  as before: 95 of 194 variants pass, 99 fail by the low parts, every one keeping 0 pixels (41 to 1399 shown; the
  barn e_wb/01 r1 0 of 844, e_wb/02 r2 0 of 913, e_shed01_3 r1 0 of 844, town houses with door steps, the dot
  pillboxes' earth). With the low parts: 194 of 194 pass, hold-out 43 of 43, every pack through the engine check;
  `test-bk-views` (with a test: a ramp on a gabled house is cut back within 3 units, a turn drawn without it keeps 0
  of its pixels and fails, with it keeps all, from behind it shows none), `bk-views-side`, `bk-views-roundtrip`.
- **Not done.** A part no 2003 side shows is not invented (the barn's far gable shows its door ramp painted on the
  wall, mirrored from the near one, as before).
- **Roof parts** (the follow-up). The barn's two ridge vents were painted flat on the roof planes in every turn.
  Cause: the fit takes them into the roof. e_wb/01's fitted ridge (69) already reaches their tops: per column its
  outline stands 0 to 2 pixels above the blocks' (measured along the whole ridge), so neither `cutChimneys` (5 pixels
  or more) nor any outline measure sees them; a mask of roof pixels coloured like the walls (vent faces are wood)
  marked rust on the barn and whole red tile roofs on the town houses, so colour does not find them either. They
  are drawn by hand: `tools/zig/fixtures/bk-views/roof-parts/<family>.json` maps a variant folder to its roof
  parts in its own frame (the hand-shape block format), measured from its 2003 picture (the vents' foot on the roof
  plane, from the pixels where their lit faces meet it: e_wb/01 x -160..-120 and -77..-34, 12 units either side of
  the ridge, flat tops at 71). Every variant of the colourway takes them, turned into its frame (`intoFrame`, the
  inverse of its eye's turn and shift), on its intact blocks (states 1 and 1w), fitted or hand-shaped: authored once
  on e_wb/01 and e_shed01_3, they stand on e_wb/02 and e_shed01_2 too.
- **Roof gate** (`roofKept`, in the shape gate): of the pixels where the turn's eye sees a wall of a roof part (the
  blocks without them behind it), 0.8 or more must be drawn as that wall (its shading class in the source map)
  wherever there are more than 40. Before (parts in the fixture, not added): 190 of 194 pass, the four barn
  variants fail keeping 0 pixels (e_wb/01 r1 0 of 438, e_wb/02 r2 0 of 504, e_shed01_3 r1 0 of 447, e_shed01_2 r2 0
  of 499); after: 194 of 194, hold-out 43 of 43. Test: a ridge vent given in the frame of a picture one turn on with
  its zero cross elsewhere comes back where it stands, and on each of the four turns the walls drawn with it are all
  kept and painted on the roof none.
- **Left.** The town houses' dormers (e_ctownhouse02/03/04, e_townhouse*, ussr town houses) stay painted on their
  roof planes, where they read as roof windows; they can be added to the roof-parts fixture the same way.

## Heaps, a planked gable and stretched pixels (M006 S07c, 2026-10-28)

The maintainer's hand test (camera turned, Remastered on) found three more turns that do not read as the 2003
object: the supply depot is a plain box with its net and crates smeared over the walls, the dug-in pillbox a flat
smeared slab, and a half-timbered house's planked gable and lean-to come out with mismatched walls.

- **Which variants.** The supply depots are the `BuildingType` 2 variants (temporary storage): `a_wt/01, /02`,
  `e_wt/01, /02`, `wt/01, /02`. Type 1 (main storage) is the barns `a_wb`, `e_wb`, `wb`, drawn well since S07b;
  type 3 the pillboxes `dot`, `dot_h`, `dzot`. No other variant is a heap of crates or earth (storehouse_1/2 and
  warehouse are houses). The house is `e_house04_4`.
- **Cause, depots and pillboxes.** A heap has no walls: a depot is crates and barrels under a net, tarpaulin or tent
  on poles, a pillbox a mound. Their hand shapes (S07) are one to five boxes fitted to the outline, so a turn paints
  the crates and earth the 2003 picture shows on the boxes' walls. A turn a sibling shows was already its sibling's
  picture; `dot` has no 2003 side 2 (`/03` and `/04` both show side 4) and `dzot/02`, `/03` have no measured side, so
  those turns were blocks: 32 turns of 14 variants (every depot turn, `dzot/02` and `/03` whole, `dzot/01` and `/04`
  r1 and r3, one turn of each `dot`).
- **Fix: heaps** (`tools/zig/fixtures/bk-views/props.json`, family to reason; `Scene.prop`, `spriteTurn`). A heap
  turns as its 2003 pictures on every turn (`sameSprite`), mirrored on an odd turn that does not keep the footprint,
  so that it spans the turned footprint's box; S06's rule for thin and small objects is unchanged. A crate may stand
  a cell off a depot's free footprint corner (e_wt's 5 x 6 footprint has two). The heaps' hand shapes stay: they
  still give the relighting classes of the mirror. A sibling's picture still comes first (`dot_h`, most `dot`).
- **No pixel measure found the heaps.** A detail-loss share (8 x 8 cells of the turned intact picture whose gradient
  energy over brightness falls below the 2003 picture's tenth percentile) spread 0.03 to 0.15 on the depots' block
  turns but up to 0.27 on real 2003 sides (dot_h/03 r3 is dot_h/01's picture), and the stretch below measured 0.001
  to 0.002 on them: the box is drawn from pixels seen squarely, only from the wrong object. So heaps are a list, as
  hand shapes are, decided from the pictures and `BuildingType`; test: `spriteTurn` draws a depot's every turn as
  its sprite, mirrored on the odd ones, keeps a house on its blocks, and every family of props.json is one of the
  inventory.
- **Cause, e_house04_4.** The fit gave the planked gable a separate slab 12 units deep and 74 high (`-184..-172`),
  whose sides the 2003 picture sees edge on, and the lean-to a flat roof. A slab's side drawn in a turn spreads each
  2003 pixel over many: 6.3% of r1's and r2's pixels. **Fix:** a hand shape (`shapes/e_house04.json`: the house a
  gable_x block -184..-42 x -23..78, walls 49, ridge 113, the lean-to -36..-5 x 4..60, walls 51, ridge 67,
  refined by `bk-views shape`; own outline IoU 0.949). The gable is now the house's own planked end wall.
- **Stretch measure and gate** (`stretchedPixels`, `stretchShare`, `stretch_most`). Per pixel of a picture
  `projectPicture` draws, the source pixel's position is kept; where a pixel and its right and lower neighbours
  take one face of one 2003 picture, the area of one target pixel in the source (the map's Jacobian) under 1/4 means
  one 2003 pixel spread over 4 or more. The gate fails a turn whose intact picture keeps more than 0.04 of its pixels
  stretched. Measured before the fix below (with the heaps and the hand shape above): 21 turns of 9 variants over
  0.04, up to 0.141 (e_railwaystation_2 r2/r3 0.14, e_shed02_5 r1/r2 0.11, e_shed02_2 0.07 to 0.11, towncornhouse01
  r1/r2 0.09/0.10, e_jettybuilding_1/_4 to 0.08, e_cityhall_1 0.06 to 0.08, ukrainehouse01_1 r2/r3 0.05; and
  e_house04_4 r1/r2 0.063 on the fit); the turns that read well 0.03 or less. sources.json carries `stretch`, the
  family reports a "Stretched" column. The first pass of the projection (S06's grazing rule) already keeps pictures
  seen under a cosine of 0.25 back where another sees the face; these are faces no picture sees better, and donor
  faces (`donorPoint`) smaller than the face they paint.
- **Fix: stretched pixels go to the patch fill.** `projectPicture` leaves every stretched pixel to `patchFill`, like
  one no picture sees: the railway station's roof, towncornhouse01's gable and e_shed02's roofs lose their streaks.
  After: 0 stretched pixels left in every turn. Tried and dropped: the smaller singular value of the Jacobian (a
  stretch along one direction) flagged walls seen at a quarter of the target's cosine (e_townrailwaystation_4 r2
  0.289), and its patch fill drew those walls as mush, worse than the stretch; and skipping a source by the ratio of
  the two cosines changed nothing (the streaks came from donors, not from grazing pictures). Test: a 6 x 6 picture
  half drawn a ninth of a source pixel per pixel has 10 of 36 stretched, over the gate, and none once filled.
- **Result** ([batch](../evidence/building-views/batch.md)): 194 of 194 variants pass, hold-out 43 of 43, 582
  folders through the engine check; 84 variants have turns drawn as 2003 sprites (76 before), 57 hand shapes (56).
- **The systematic pass** (contact sheets of all 194 before and after, `zig-out/local-test/s07c/contact-before` and
  `contact-after`, not tracked). Fixed here: the six depots, `dzot/01..04`, `dot/01..04`'s block turn, e_house04_4,
  and the stretched turns above. Still not reading as the 2003 object, by eye, left for the next slices with their
  kind (no measure separates them yet):
  - roof parts painted on roof planes (S07b's open item): dormers and chimneys of the ussr town houses
    (townhouse03_1/_2, 04_1/_2, 05_1/_2/_3, towncornhouse03/04), e_ctownhouse02/03/04, e_townhouse*; fix: roof-parts
    fixture entries;
  - a wall or roof taking another material: blue-green walls on e_ctownhouse03_2/_4 r1 and r3 and e_house05_3 r1/r3,
    greenish walls on e_townhouse04_3, red roof stripes on the walls of e_townhouse01_3/_4 and e_chapel_1 r2/r3;
    fix: a source choice by colour against the face's own 2003 pixels;
  - bleached patches where a roof plane is relit from a few pixels: e_frenchshed06's hip ends, e_ctownhouse01_1 r1/r3,
    e_ctownhouse02_1 r3, e_house11_1 r3, a_h03_2/_3 r2/r3;
  - jumbled multi-wing roofs: e_cityhall_1 r2/r3, e_jettybuilding_1/_4; and a_h04_1 r1/r3's streaked arcade;
  - porch slabs that take roof texture: e_house03_3/_4 r1.

## S07c's list, light and copied parts (M006 S07d, 2026-10-28)

S07c left six kinds of turns that do not read as the 2003 object; the maintainer added a seventh: a face no 2003
picture shows is copied from the opposite one, so a house's back repeats its front's chimney, dormers and door ("the
buildings do not turn").

- **Light** (`shading`, `relight`; rows: walls in the wrong material, bleached patches). Cause: a face drawn for
  another side was relit by the ratio of the mean colours of the two shading classes over the 2003 pictures (`Means`).
  Those means took the materials along: e_ctownhouse03_2's shaded wall is stone and its lit wall plaster, so the stone
  turned to the light came out blue-green (class means (74, 49, 26) against (135, 111, 72)); roof pixels that a box
  model puts in a wall's class turned walls grey or red; a class seen on a few hundred pixels set the gain of a whole
  roof plane (the bleached hip ends). Fix: the intact pictures are relit by the 2003 light itself, an ambient and a
  diffuse part towards `light_2003`, per channel with the ratio of the lit wall to the shaded one measured in S08
  (`wall_light_rgb` 1.82, 2.03, 2.71: the sun is yellower than the sky that lights the shade; one factor turned
  a_h02_2's shaded cream walls a saturated orange in the light). The model predicts the 2003 roofs: the roof towards -y
  against the one towards +x measures 1.26 to 1.42 on e_ctownhouse02_1, e_house02_1, e_house10_2 and e_house11_1
  (model 1.40 at a pitch of 1). A relit pixel keeps its hue where a channel would clip. Damage states keep the class
  gains (S07): a ruin's fitted blocks stand for rubble, and relit by the light their faces took seams (e_house07_1's
  ruin sharpness 1.13 to 2.53 against the band's 1.55). A mirrored heap is not relit (row 7: the mirrored e_wt depots
  were 5% darker than the 2003 picture, now 1.000 of its mean brightness); other mirrored props keep S07's relighting.
- **Tried and dropped: a gate measure on wall colour.** The chromaticity of each wall face of a turn against the hull
  of those the 2003 pictures show (per class and per face). It did not separate the rows: before the light fix the
  listed walls measured 0.003 to 0.056 and sound turns up to 0.078 (barracks_2 r3), the material of a single
  building's walls spreads as far as a wrong source's. A source choice by colour (a pixel coloured like the other
  material of its picture passed over) was tried too: it turned red roofs and rust grey (e_townhouse01_3's red
  stripes are 2003 roof pixels, (195, 0, 0), but so are factoryfloor02's rusty roof's); dropped. The light model
  removes the cause instead; the rows that are geometry (roof pixels on a wall that a box puts there) are hand shapes
  and roof parts below.
- **Copied parts** (`nearPart`, `copiedParts`; the maintainer's row). A face of the body does not borrow a donor point
  within 6 units of a low or roof part's rectangle and up to 40 above its top (a door over its step, a dormer's
  flashing); the patch fill or the face's next donor stands in. Gate measure `copied`: the pixels of the body a turn
  draws from such a point; a turn fails above 40 (as the low parts). With the exclusion off, 225 turns of 90 variants
  copy more than 40 (up to 9208, townhouse03_1 r2); with it, 0. Intact states only: without their steps' surroundings
  the ruins of e_shed02_2 and townhouse02_1 lost their hue (0.466 and 0.332 against 0.29). sources.json carries
  `copied`, the family reports a "Copied" column. Test: a gabled house with a chimney on its front roof seen from
  behind copies none of the chimney's surroundings with the chimney a part and more than 40 without, and still draws
  over 1000 pixels of its back roof from the front.
- **Roof parts and hand shapes** (rows 1 and 4 to 6). Measured on the 2003 pictures with a scratch helper (the roof
  plane under a picture pixel from the blocks, the heights from the pixel rows, as S07b): dormers (gabled blocks
  whose ridge runs down the roof) and chimneys (flat blocks from the ground) in `roof-parts/` for townhouse03/04/05,
  towncornhouse03/04, e_ctownhouse02/03/04, e_townhouse01/02/04, e_house03, a_h04 and e_jettybuilding; on a building
  symmetric about its ridge (the ussr town houses) the dormers are mirrored to the far roof plane, which the 2003
  picture hides. New hand shapes: townhouse03/04/05 (hipped or gabled main block that the fit had cut flat),
  towncornhouse03 (two hipped wings), e_townhouse04 (one steep gable), e_ctownhouse03_2 (a cross gable over the
  front), e_cityhall_1 (cross wings at both ends: the jumbled roof was the tower's and the gables' pixels on the main
  roof), e_jettybuilding (its tower and clock gable moved to roof parts, so the copied roof no longer repeats them;
  _4 takes _1's shape turned), e_house03 (the porch a gabled block instead of a flat slab that took the roof: row 6),
  e_frenchshed06 (one gable instead of a flat box with a gabled end: its bleached hip ends), a_h03 (stepped flat
  roofs instead of a hip), a_h04 (a notch with three steps for the outside stair: row 5). `shape` and the hand shape
  test now measure the hand shape with its roof parts (a tower or gable moved there still counts).
- **Result** ([batch](../evidence/building-views/batch.md)): 194 of 194 variants pass, hold-out 43 of 43, 582 folders
  through the engine check, 73 hand shapes (57), roof parts on 85 turns of 31 variants with 150523 of 150830 wall
  pixels kept, 0 copied part pixels. Contact sheets before and after: `zig-out/local-test/s07d/contact-before`,
  `contact-after` (not tracked).
- **Not done.** Row 7's crate: a mirror across the screen's vertical maps the footprint (x, y) to (-y, -x), the turn to
  (-y, x); e_wt's 5 x 6 footprint has free corners that the two put in different cells, and no shift of the mirrored
  picture lays it on the turned one. Writing the mirrored footprint into the turned 1.xml would move the depot's
  passability off the 2003 object's, so it is left as a known offset of one cell.
- **Still not reading as the 2003 object** (the pass over all 194 after the fix): a_h04_1 r1/r3 (the gallery's and
  terrace's railings land on the flat roofs as stripes; a railing is no block), e_jettybuilding_1/_4 (patch-filled
  roof planes, a building of five roofs from one side), e_townhouse02_2/_3 (its large dormers are only partly in
  the fixture; the roof between them is patch fill), e_house07_1 (the roof's eaves on the gable walls, whiter in the
  light), e_house01_1/_3 (a thatch roof seen from the shaded side relit bright, as before), e_townhouse01_3/_4 r2/r3
  (the porch roof stands as a red slab), e_house11_1 r3 (its lean-to's roof lit pale), a_wb/01 r1/r3 (planked gables
  pale in the light).

## Open questions

- How the Game should pick a variant for an object placed in a map at orientation k under camera step r
  (variant (k + r) mod 4), including the families whose variants differ in colourway.
