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
- `.san` layout (decoded for the study, to be confirmed against `Sources/src/ResourceModel` and
  `Sources/src/Scene/VisObjBuilder.cpp`): records of 9 floats `x, y, size, u1, v1, u2, v2, d1, d2` from offset
  0x24 place square texture pieces of 16 or 32 px on screen; the texture `1_c.dds` is an atlas of those pieces.
- Blitzkrieg 2: 135 building models in Maya sources (`Complete/Buildings/<region>/<name>/*.mb`, not
  readable by open tools) and compiled geometry (`Versions/Current/Data/bin/Geometries/<uid>`, named by each
  building's `summer_<n>_geometry.xdb`), with damaged and destroyed states. 32 of our 116 family names match a
  Blitzkrieg 2 model exactly; more match under another spelling (`barracks`/`barraks`, `admhouse`/`admbuilding`).

## Pipeline

1. **Decompose** (`bk-sprite decompose`, Zig): a `.san` plus its `.dds` become a PNG per animation frame and
   damage state, with the screen anchor, the depth values and the footprint from `1.xml`.
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
10. **Compose** (`bk-sprite compose`, existing): renders back into `.san`/`.dds` variants beside the originals;
    each orientation keeps (or gets, rotated) its footprint data.

## Slices

1. **Decompose and camera:** `bk-sprite decompose` with a round-trip test (decompose, compose, byte-compare the
   frames), and the camera fit on the 3-side families, measured by outline overlap.
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

## Open questions

- The exact `.san` header and chunk layout (the study decoder stopped before the end of some files).
- Whether the 2003 camera is a pure orthographic projection; the camera fit answers it.
- How the Game should pick a variant for an object placed in a map at orientation k under camera step r
  (variant (k + r) mod 4), including the families whose variants differ in colourway.
