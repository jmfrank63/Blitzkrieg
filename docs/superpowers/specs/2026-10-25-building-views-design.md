# Building views for a rotatable camera (Remastered)

Blitzkrieg draws every building, bridge, fence and most objects as a sprite rendered once from the
game's fixed camera. Vehicles and guns are 3D models (`Data/Units/Technics`: 500 `.mod`) and turn
freely; infantry has 8 facings. A camera that turns in 4 (N, E, S, W) or 8 steps therefore needs the
sides of every sprite object that were never drawn. This project fills them, so that the Remastered
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
9. **Render** (Blender): 4 (or 8) views per building, damage states, shadow layers, seasons, at the solved
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
7. **The camera in the Game** (Remastered only): 4/8 rotation steps, draw order per angle, picking, scrolling,
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
compose-pack again byte for byte, decompose back).

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

## Open questions

- How the Game should pick a variant for an object placed in a map at orientation k under camera step r
  (variant (k + r) mod 4), including the families whose variants differ in colourway.
