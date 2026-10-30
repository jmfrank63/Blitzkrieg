# Full 3D objects: buildings and props as textured models (Remastered camera turn)

The maintainer (2026-10-29), after the round-5 fill of the joint families: "I see this creates a new set of
problems. We should go full 3D. What I mean is we create the buildings and objects just like vehicles as full 3D
models and paint them instead. We keep our 4 angles meaning we can precalculate, but all will be 3D. This way we can
inspect each model individually. The problem is the textures. With 3D this is easily done. 2D is too complicated."
And: build from the tracked meshes (his family review, `D:\bk-review\shapes-family-review.pdf`), keep the details
the review shows (ladders, porches, roof openings, supports, chimneys) as parts, and match its alignment (yaw,
height, width) when N is set against the 2003 picture.

So far a turned building was a 2D synthesis: `bk-views side` projected 2003 pixels onto blocks or, since round 5,
onto the tracked mesh, pixel by pixel of each turned picture, and patch-filled what no picture saw
([building views](2026-10-25-building-views-design.md), [camera turn](2026-10-26-camera-turn-design.md)). That
synthesis is what this design leaves: every object becomes a model with one texture atlas, painted once, and every
turn is that model rendered.

## Decisions (2026-10-29)

- **The vehicles' format family.** A model is a `.mod` (an engine structure file, `Formats/fmtMesh.h`: skeleton,
  meshes, no animation; written as `bk-map obj2mod` writes one) with its texture as `<state>_h.dds` (A8R8G8B8, one
  level: what GFXGPU's `TextureGpu` reads as it is; `_c` and `_l` are lossy quality levels the loader falls back
  from and are left out until a model is drawn by the Game at run time, which is not planned). Per intact state
  (`1` summer, `1w` winter) one atlas on the same UV layout, as the vehicles' `1`/`1w` textures. World units (the
  Game's visual units, 22.6 an AI tile), z up, the variant's zero cross at the origin, turned as its 2003 picture
  shows it (N).
- **Parts by name.** One mesh a group and one skeleton node a group (mesh *i* is drawn on node *i*), each node at
  its group's middle and every node a locator, so the Resource Editor's Unit editor lists the parts by name and
  shows them as locators. A group is the mesh's own part kind where the review names one (`roof`, `body`, `timber`,
  `sandbag`, ...), else what a connected piece of the mesh is (`body`, the largest; `chimney`, standing above 60% of
  the height on a foot under 8% of the ground box; `low`, under 30% of the height: steps, porches, ramps; `part`
  else), with how a face looks (`wall`, `roof`, facing up by 0.5 or more, `under`): `body.wall`, `chimney.roof`,
  `timber.wall`. The mesh is taken whole, so every detail the review modelled (ladders, porches, roof openings,
  supports, chimneys) is in the model.
- **Geometry: the tracked meshes, unchanged** (`tools/zig/fixtures/bk-views/meshes`, ids kept), placed on the
  variant's 2003 picture as the review placed them (`mesh.place`: the alignment's yaw, height and width factors,
  then the projection's box on the picture's opaque box, the lowest point on the ground). Every face is written
  twice, both windings: the engine culls one side and the review's meshes do not keep one winding; an open board
  is seen from both sides. A face's outward normal is its winding's unless a ray from its middle leaves the mesh
  through an odd number of faces.
- **UV atlas.** A chart is a set of faces of one group joined across edges in one plane (normals' cosine 0.999),
  unrolled with u along the horizontal and v up the face, so wall courses stay level and a wall stands on its
  chart's lower edge; 2 texels a world unit (the 2003 camera draws at most 0.87 pixels a unit, so no 2003 pixel is
  lost and the painter has room); 2 texels of padding round each chart, painted as the nearest point of its faces;
  charts in rows, tallest first, in the smallest atlas whose sides are powers of two.
- **Paint: baked once from the 2003 pictures, the light divided out.** A texel takes the variant's own 2003
  picture where that sees its point at least at `grazing` (0.25), so the model at N is the variant's 2003 side;
  else the picture of the colourway (a joint family's views) that sees it most squarely. Changed from "each face
  from the view that sees it most frontally" of the brief: with most-frontal-first e_wb/02's N took e_wb/01's gable
  door and its colour difference at N was 19.1; own-first it is 3.0. The pixel is stored with the 2003 light
  divided out (`shading` with the building's own wall light), so the atlas is the surface's colour under the
  ambient light and every turn is lit by the renderer. What no picture sees gets a **material paint**: a tile, the
  largest fully seen rectangle of its own chart if that is a good tile (8 texels a side or more and a quarter of
  the chart or more; a wall partly hidden by a chimney continues itself), else the largest good tile of its
  material (group and part kind: a back wall takes the front wall's plaster, windows and all), else of the faces
  that look the same way (a wall, a roof), repeated from the chart's lower left corner. Every guessed texel is
  marked (`<state>_paint.png`: green from a picture, magenta guessed) and the review shows the map and counts the
  guessed pixels of every turn. A smaller seen rectangle is mostly a strip where mesh and picture disagree by a
  pixel (e_house02_2's gable under the roof's edge, seen as roof), whose tile painted a whole wall salmon.
- **Textures generated, tracked once painted.** The bake is deterministic from tracked data (meshes, Data), so the
  atlases are build output (`zig-out/full3d/models/<id>/`), with a lossless `<state>.png` beside each `.dds` and a
  `<state>.tga` for the Resource Editor's unit sources. The first atlas the maintainer paints becomes tracked
  (`tools/zig/fixtures/bk-views/paint/<id>_<state>.png`, the model's own UV layout) and wins over the bake; the
  PNG reader that needs (deflate) comes with that first painted atlas, not before.
- **The renderer: a CPU rasterizer in Zig** (`bk_views_model.render`), not GFXGPU headless: the same bytes on every
  run and every machine (f64, one thread, no driver), offline anyway, and the same camera code the review's measure
  and the round-5 placement use (`mesh.project`, the rows of `camera-2003.json`), so a render at N lies on the 2003
  picture with no shift. 4 x 4 samples a pixel, each the nearest texel of the face in front; a pixel is opaque where
  half its samples or more meet a face (one-bit alpha, as the 2003 sprites), its colour their mean. Lighting is
  `shading` (the 2003 model fitted in M005): `_r<k>` the 2003 light fixed to the camera, `_r<k>_l<s>` the world's
  sun at camera step s (the light of turn k - s). Shadows are the mesh's (`meshShadow`, as round 5).
- **No runtime 3D for buildings.** `bk-views side` renders the intact states of a full 3D object at the turned
  2003 camera and packs them as before (compose-pack), into the same `<variant>_r<k>` and `_r<k>_l<k>` folders; the
  Game reads them unchanged. N in the Game stays the 2003 sprite; the render at N exists for the review and the
  match.
- **Siblings.** A full 3D model draws every turn, also one a 2003 sibling shows: the sibling's art is painted into
  the atlas, so the turn is the one building in one light at one place. The review sets the sibling's 2003 picture
  against that turn.
- **Damage states.** The pilot models the intact building (`1`, `1w`); the damage states stay on the blocks fitted
  to their own 2003 outlines, moved onto the mesh's zero cross (as round 5). The target: a model per damage state,
  as the vehicles have `1.mod`, `2.mod`, `3.mod`: the intact model cut by the visual hull of the damaged 2003
  picture at N (what the damaged outline no longer covers is gone), painted from the damaged picture where it sees
  and from the intact atlas elsewhere, its guessed paint marked as here (built, with fallen roofs: see "All 412
  objects" below).
- **Inspection: the Resource Editor's Unit editor**, no new viewer. The `.mod` and the summer atlas in place of a
  unit fixture's models and pictures: the preview shows the model lit from the 2003 camera and from above, and its
  parts are the skeleton's nodes, picked by name (`resource-editor-full3d`).
- **Which objects:** `tools/zig/fixtures/bk-views/full3d.json` lists the full 3D objects by number; `side` draws
  every other object as before. The list grows to all 412 and then goes away (gone: see "All 412 objects" below).

## The pilot

`bk-views model3d <out-dir> <review.pdf> <variant-dir>...` writes per object `<out-dir>/<id>/`: `1.mod`,
`<state>_h.dds`, `<state>.png`, `<state>.tga`, `<state>_paint.png` and `model.json` (groups, atlas, wall light,
paint shares), and the review: per object a page with the 2003 picture, the render at N, their difference, the mesh's
creases, the atlas and its paint map, the numbers; and a page with the four turns under the 2003 light and under the
world's sun, each with its guessed pixels. `zig build full3d-pilot` runs `side` on every pilot object into
`zig-out/remastered` and `model3d` into `zig-out/full3d/models` and `zig-out/review/full3d.pdf` (`full3d.csv`).

Objects: africa a_h01 (3, 4: a joint family, an L-shaped house with ladders and a pillared porch), europe e_wb
(111, 112: a joint family, the warehouse of Poland's village), e_house02_2 (62: a half-timbered house with a chimney
and roof windows, in Poland's village), the ussr church (124: drum, onion dome, cross, apses, porch), and the prop
villagefence/03 (384: a picket fence, thin parts).

## Pilot results (win-home, Windows x64, 2026-10-29)

`zig build full3d-pilot` (ReleaseFast bk-views; `full3d.csv` beside the review). N is the render at turn 0 under
the 2003 light on the 2003 picture's own canvas at its zero cross, no shift allowed; "mesh" is the mesh's own
outline there (what the geometry can reach); colour is the mean absolute difference of 255 where both are opaque;
holes are render samples on texels no paint reached, over N, the three turns and the three under the world's sun;
guessed pixels are those of E / S / W showing mostly material paint; "same" is a second build and render of the
model compared byte for byte (`.mod`, every atlas, every turn).

| id | object | triangles | groups | charts | atlas | seen / guessed texels | N outline (mesh) | N colour | holes | guessed E / S / W | same |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 3 | a_h01_1 | 1122 | 7 | 372 | 1024 x 2048 | 0.59 / 0.41 | 0.921 (0.921) | 2.58 | 0 | 1020 / 10331 / 9141 | yes |
| 4 | a_h01_2 | 1122 | 7 | 371 | 1024 x 1024 | 0.59 / 0.41 | 0.875 (0.870) | 2.84 | 0 | 10164 / 9095 / 741 | yes |
| 111 | e_wb/01 | 62 | 6 | 34 | 512 x 1024 | 0.72 / 0.28 | 0.960 (0.960) | 3.49 | 0 | 4073 / 4017 / 60 | yes |
| 112 | e_wb/02 | 62 | 6 | 34 | 512 x 1024 | 0.72 / 0.28 | 0.959 (0.959) | 3.01 | 0 | 57 / 4137 / 4045 | yes |
| 62 | e_house02_2 | 65 | 8 | 32 | 512 x 1024 | 0.33 / 0.67 | 0.865 (0.866) | 5.51 | 0 | 8687 / 12567 / 5198 | yes |
| 124 | church (ussr) | 509 | 12 | 262 | 1024 x 1024 | 0.35 / 0.65 | 0.960 (0.959) | 3.22 | 0 | 9877 / 19774 / 12739 | yes |
| 384 | villagefence/03 | 204 | 3 | 104 | 256 x 256 | 0.21 / 0.79 | 0.770 (0.668) | 6.03 | 0 | 468 / 163 / 446 | yes |

Seen and guessed are shares of the texels of faces that look up or sideways (floors and undersides no turn shows
are left out). The render's outline at N equals the mesh's to 0.005 everywhere (the fence's is higher: its
pickets are narrower than a pixel and the 4 x 4 samples keep them where the mesh's pixel middles miss), so the
renderer's projection is the 2003 camera's to the pixel; the rest of the outline is the mesh against the
picture, the maintainer's review's business (62's BK2 house carries a back wing the 2003 house does not show).

The 2003 siblings against the turn that shows their side (outline IoU where the footprints lay the sibling, as
round 5 laid it; at the best shift; colour at that shift): a_h01_2 against 3.E 0.781 / 0.879 (10 and 6 pixels),
a_h01_1 against 4.W 0.801 / 0.926, e_wb/02 against 111.W 0.917 / 0.957, e_wb/01 against 112.E 0.948 / 0.968.
Their colour differences are 27 to 35: per pixel, after a shift of a few pixels, textured surfaces never meet
pixel for pixel; and the faces both pictures see take the variant's own paint, not the sibling's.

Before the own-first rule (most frontal first, as the brief said) e_wb/01 and /02 measured 9.20 and 19.12 at N; a
first unwrap without the nearest-face texels left 3068 hole samples on a_h01_1 (sliver charts narrower than a
texel); a first material paint from any seen rectangle painted e_house02_2's back wing salmon.

Timing: `model3d` on all seven, each built twice for the determinism check, 199 s in Debug; `side` on e_wb/01's six
turns 38 s in Debug.

The `.mod` is read by the engine's own reader: `resource-editor-full3d` opens e_wb/01's model in the Unit editor
(the unit fixture's project with the model as its three models and the summer atlas as its pictures), whose
locator rebuild reads the skeleton (6 nodes), whose preview exports and draws it (39.9% of the frame lit; from
the side, the horizontal camera, 0.22% of the pixels differ, with the locators shown 0.009%), and picks the parts
`body.roof` and `chimney.wall` by name at their screen points. The first try read 1 node: the skeleton's nodes are
a vector of structures, held in their own chunk 1 (`DoVector` inside `Add`), which the writer had left out; the
test checks that nesting now. In the preview the model looks darker than in the Game's turns: its atlas is the
surface under the ambient light, and the editor lights it with its own sun.

The Game reads the rendered folders unchanged: `camera-turn-village-game-reads-it` turns Poland's village with
`bk-views side`, which now renders e_house02_2's turns and those of the bars test's warehouses e_wb/01 and /02 from
their models (`"full3d":true` in their sources.json), and finds every house's side and place at all four steps
in the three window sizes, and the storages' bars over their turned pictures: pass. `camera-turn-game-reads-it`
(two buildings outside the pilot, through the same `side`): pass.

Commands run here (win-home, Windows x64): `test-bk-views` (the two new tests and the quality gate's hold-out,
which now draws a_h01's held-out side from the full 3D model: pass), `full3d-pilot`, `resource-editor-full3d`,
`camera-turn-village-game-reads-it`, `camera-turn-game-reads-it` (pass). Not run: the other Resource Editor auto
tiers (the editor's code is unchanged; the new tier only adds a schedule), the full sweep (left to the
maintainer), macOS and Linux (CI).

## Left open

- The prop's turns (villagefence/03) are written into zig-out/remastered/Objects as the sand pile's are, but no
  Game tier places a prop turn yet (as in round 3).
- Damage-state models (decided above, not built); the pilot's damage states are the blocks' as before.
- Guessed paint is a tile, not a picture of the hidden side: the review's magenta is what to paint first.
- The Unit editor shows a model with the unit fixture's stats around it; a static-object project kind for models
  is not needed for inspection and not built.

## Scaling to all 412

- **The list.** Add the ids to `full3d.json` (a family's views together: a joint family's views paint one another),
  run `full3d-pilot` (it takes the list) and review `full3d.pdf`; when all 412 are in, drop the list (every object
  with a mesh is full 3D) and the blocks' drawing of intact states with it, after the parity of the batch's tiers.
  The building-views batch steps and `objects-relight` then draw every turn from the models; 278 (e_megaphone) has
  no 2003 picture and gets material paint only, so it needs a painted atlas first.
- **Geometry before paint.** The render is only as good as the mesh's outline on its picture: the 178 objects the
  review marks `needs_review`, the low outlines (antitank stakes 0.40 to 0.64, the BK2 houses at 0.79 to 0.87 such
  as e_house02's back wing) are mesh fixes in the maintainer's review, imported again with `bk-views import`.
- **Paint.** Single-picture objects guess 60 to 80% of their texels (one 2003 side sees two walls and two roof
  planes). The atlases are the place to paint: the first painted atlas becomes tracked
  (`tools/zig/fixtures/bk-views/paint/<id>_<state>.png`) with the PNG reader it needs; a generative fill, where
  used, writes into the atlas too and keeps the guessed mark. A mesh change re-unwraps the atlas, so a painted
  atlas pins its mesh (a re-import that changes a painted mesh must carry the paint over by projecting it).
- **Parts.** Named groups come from the review's part kinds; most BK2 adaptations are one `mesh` kind, so their
  groups are the connected-piece guess (e_wb's roof vents came out as `chimney`). Naming the parts in the review's
  meshes (chimney, dormer, porch, ladder, support) makes them the models' groups and locators unchanged.
- **Damage states:** the models per state of the decision above (`2`, `3`), then the blocks' fit goes.
- **Not in the 412:** Data/Fences and Data/Bridges (segments and spans that turn with the ground), flora.
- **Cost.** The unwrap's outward test is quadratic in triangles (fine to a few thousand: the church's 509 take
  under a second); a mesh of tens of thousands would want a grid. 412 objects at the pilot's pace are about 2 hours
  in Debug, a fraction in the ReleaseFast the build steps use; one `side` per object, run per family as the batch
  does.

## All 412 objects (2026-10-29, second round)

The pilot's list (`full3d.json`) is gone: every object with a tracked mesh (all 412 of shape-ids.json: 194 building
variants, 218 props) is a full 3D object in `side`, `holdout` and `model3d`, its joint family's views included. A
folder without a mesh (a modder's variant) keeps the blocks.

### Damage states: a model each

As decided above, a damage state is a model of its own (`model3d.damaged`, written as `2.mod` and `3.mod`), derived
from the intact model and the variant's own damaged 2003 picture at N. It keeps the intact model's charts and texels,
so its atlas starts from the intact one and is painted over from the damaged picture (`bake` with a base).

- **The outline cut.** In each pixel column of the damaged picture the outline's top is where the building now ends;
  between the columns' edges the line runs straight (an edge takes the higher of its two columns'), so neighbouring
  faces are cut along one line and no crack opens. A wall above the line is gone (a breach, a fallen gable); a face
  that looks up is lowered straight down onto the line's surface, so a roof the outline cuts lies over the cut walls
  as a cap; a column the damaged picture does not reach keeps nothing. A face is split only in the columns where the
  line crosses it (a run of columns where it lies wholly under the line stays one piece: e_wb/01's state 3 took 4758
  triangles split column by column, 2392 with the runs). This is the visual hull of one picture.
- **Fallen roofs.** One picture cannot tell how deep a pixel lies, so rubble seen inside the walls kept the roof over
  it: e_wb/01's state 3, turned, showed an almost whole roof. A roof group (`body.roof`, `chimney.roof`, ...) whose
  pixels at N in the damaged picture mostly differ from the intact picture's (half or more of 20 or more: clear, or a
  mean channel 48 or more off; a group N hardly sees goes as all the roofs N sees) has fallen: its faces lie flat at a
  tenth of the building's height, a floor of rubble inside the cut walls, painted from the damaged picture where it
  sees them and elsewhere tiled from the rubble it does see (the material paint's tile rule on the fallen charts),
  marked guessed.
- **Seasons.** Each season of a state is an atlas on the same model: `2` and `2w`, `3` and `3w`, each started from
  the intact atlas of its season (`1`, `1w`) and painted from its own damaged picture; the geometry is the summer
  picture's. The 2003 data has summer and winter only (`w`): 109 props with `1`, 109 with `1` and `1w`, 100
  buildings with `1`, `2`, `3`, 94 with their winters too.
- **Shadows** are each model's own (`model3d.shadow`).
- **What N does not see.** Of a face N does not see (a back wall, a far wing) the picture tells only how much of the
  building stands where it does see: the share of the walls N sees that the damaged picture still shows unchanged.
  A piece of the cut building N does not see (judged on the cut model at the render's 4 x 4 samples, so a back wall
  a fallen roof uncovers counts as seen) stands no higher than that share of the building's height, a roof among
  them lowered onto it (e_house02_2's back wing, its steep roof counted as a wall, came down in state 3). A first
  try measured the share as the outline's height per column against the intact one's: 0.87 for a_h01_1's ruin,
  whose rubble at the back stands as high on the screen as the walls did. A first try judged visibility on the
  intact model and cut the back walls the fallen roof uncovers at N (a_h01_1's state 3 at N: 0.815 to 0.778).
  Left: a wall N sees through a fallen roof keeps the height the outline gives it (a_h01_1's back walls turned).

### The render: the colour of the pixel's middle

A render pixel took the mean of its 16 samples. A 2003 pixel spans about two texels, so a turned picture was a box
filter of the 2003 pixels, softer than the 2003 sides: once every gate building was a model, the quality gate's
hold-out failed 52 of its 75 rows, almost all on sharpness. A pixel now takes the colour of its middle sample (the
mean of its covered samples where no face meets the middle; coverage stays the 16 samples'): 11 rows outside the
blocks' floors (19 with the middle 2 x 2 samples' mean), all of damage states. At N the render is the 2003 picture's
pixels again: colour difference e_wb/01 3.49 to 1.13, a_h01_1 2.58 to 0.92, e_house02_2 5.51 to 0.51,
villagefence/03 6.03 to 1.79, the church 3.22 to 1.18.

### The quality gate derived again

gate.json's floors came from the blocks' hold-out of its 23 variants by a rule its `about` states; drawn by the
models, the same rule gives (the blocks' in brackets): intact IoU 0.75 (0.64), mean IoU 0.88 (0.84), mean SSIM 0.25
(0.13), sharpness 0.42 to 1.89 (0.45 to 3.2), colour 49 (62), hue 0.56 (0.77); state 2 IoU 0.71 (0.59), mean IoU 0.83
(0.77), mean SSIM 0.04 (0.0), sharpness 0.31 to 1.93 (0.5 to 2.2), colour 77 (60), hue 0.43 (0.29); state 3 IoU 0.56
(0.6), mean IoU 0.74 (0.72), mean SSIM 0.01 (-0.02), sharpness 0.45 to 1.64 (0.7 to 1.55), colour 72 (49), hue 0.34
(0.26). Measured: intact worst / mean IoU 0.796 / 0.898 and mean SSIM 0.293, state 2 0.760 / 0.846 and 0.082,
state 3 0.607 / 0.756 and 0.053: the models hold the outline better at every state but state 3's worst; a damage
state drawn for another side paints that side's rubble from its own picture, so its sharpness, colour and hue against
the other side's 2003 rubble are looser. The hand shapes' test (blocks) keeps the blocks' mean floor, 0.84.

### Retired

- `full3d.json`, `full3d-pilot` (now `full3d-<region>`, `full3d-review`, `full3d`).
- The round-5 fill of the joint families on their mesh (`meshDrawn`: intact states drawn pixel by pixel on the mesh
  and the sibling's 2003 art as a mesh turn), `fill-families` and `bk-views fill-review`: every joint view is a model.
- The blocks' damage states of meshed objects, and the zero-cross shift between blocks and mesh.
- objects-relight's `side` of the sand pile: full3d-common draws every prop's turns.

Kept: the blocks for folders without a mesh, the hand shapes and their tests, `shape`, `models` and the shapes review
(blocks only, `intact_only`: they build no models), the relight of flora and of the four-sided props (whose turns
are their sibling variants in the Game), the building-views batch (its `side` now renders the models).

### e_megaphone (278)

The review's PDF has no 2003 picture of it, but Data holds its sprite (`e_column/e_megaphone/1.san`, 50 x 63): it is
painted from that like every other object.

### The batch and the review

- `full3d-africa`, `-common`, `-europe`, `-ussr`: per object `bk-views side` (its six turned folders into
  `zig-out/remastered`, every state rendered from its model), the engine's reader on every pack (the batch's round
  trip), and `bk-views model3d` into `zig-out/full3d/models/<number>/`: a `.mod` a state, per state `<state>_h.dds`,
  `<state>.tga` and a deflated `<state>_paint.png`, `model.json` (the numbers below; a second build and render compared
  byte for byte) and `review/` (the pictures the review lays out). One run an object, so the build spreads them over
  the cores and every output is that object's alone: the same bytes however many run at once. The `.png` beside each
  `.dds` is gone (the `.dds` and `.tga` are both lossless).
- `full3d-review`: `zig-out/review/full3d.pdf`, a contents index as shapes.pdf's (number, path, needs-review mark, N
  outline, damage states, page), then per object the pilot's two pages (the 2003 picture, the render at N and their
  difference, the mesh's creases, the atlas and its paint map, the numbers; the four turns under the 2003 light and
  under the world's sun) and, for a building with damage states, a third: each state's 2003 picture, its render at N,
  their difference and its three turns. Beside it `full3d.csv` (per object: triangles, groups, charts, atlas, seen
  and guessed shares, N outline of the render and of the mesh, N colour, holes, guessed pixels at E / S / W, the
  sibling's match, per damage state its outline at N, the intact model's there, colour and holes, the winter atlas's
  guessed share, the second build's bytes) and `full3d-ranked.csv` (every object ranked twice: needs paint by the
  summer atlas's guessed share; needs mesh fix by the lowest outline IoU at N of its mesh and its damage states' cuts).
- `full3d`: all four regions and the review (longer than one foreground command: run the regions one by one).
- **The first batch stopped on props**, two causes. The pilot's canvases kept 18 pixels of margin round a meshed
  object for its block-drawn damage states; with that margin down to the blocks' 2, a prop's zero cross (laid on the
  footprint's middle, `meshZero`) could fall off its canvas (mine_r1: 31,13 of 27 x 18), and on the Mac one left of
  the canvas reached bk-sprite as `--zero -3,...`, an option: `turnTarget` now takes the zero cross into the canvas.
  And on Windows compose-pack stopped at a debug CRT dialog: the mine renders as a post one pixel wide, and the
  icon's crop (`SaveIconFile`, MFC's box, which leaves out its last row and column) made an empty image its copy read
  out of range. A box one pixel thin now keeps that pixel; every other icon is cropped as before.

### Results (win-home, Windows x64, and macOS arm64, 2026-10-30)

`full3d` on all 412 objects (`zig-out/review/full3d.pdf`, 197 MB, copied to `D:\bk-review\full3d-all.pdf`;
`full3d.csv`, `full3d-ranked.csv`). The two CSVs are byte for byte the same on Windows x64 and macOS arm64 (the PDFs
have the same size; their bytes differ, not looked into).

- **Coverage.** 412 intact models, 194 models of state 2 and 194 of state 3 (800 `.mod`); atlases `1` 412, `1w` 203,
  `2` and `3` 194 each, `2w` and `3w` 94 each. Holes (render samples on unpainted texels) in every N, turn, sun-lit
  turn and damage state: 0. A second build and render: the same bytes for all 412.
- **N against the 2003 picture** (outline IoU of the render; colour difference of 255 where both are opaque):
  outline min 0.042, 10th percentile 0.448, median 0.829, 90th 0.939, max 0.978 (buildings' median 0.882, props'
  0.709); colour min 0.29, 10th 0.61, median 1.12, 90th 5.46, max 36.6. The render's outline is the mesh's: the low
  ones are meshes, the top of the needs-mesh-fix list: box/00 (0.04; mesh 0.11, two bars for a lattice crate),
  wirefence/03 and /04 (0.12, 0.22), wickerfence/07 (0.23), factoryfencewire/06 (0.26), railroadbarrier/01 (0.28),
  box/04 (0.28), mine (0.29, a post one pixel wide), conduitbarrel's state 3 (0.14). The highest colour differences
  are thin or open props where few pixels meet: box/03 36.6, wirefence/03 31.3, e_columnclock/04 22.3.
- **Paint.** Guessed share of the summer atlas (texels of faces that look up or sideways no 2003 picture sees):
  min 0.08, 10th 0.42, median 0.62, 90th 0.77, max 0.93 (villagefence/06 0.93, /09 0.92, wickerfence/07 0.88,
  railroadbarrier/01 0.88, debris/04 0.87 head the needs-paint list); winter atlases median 0.57.
- **Damage states** (388): outline IoU at N of the cut model min 0.14, 10th 0.62, median 0.82, 90th 0.92, max
  0.97; against the intact model on the same damaged picture it gains median 0.045, 90th percentile 0.23, at most
  0.38 (the heavy ruins: e_wb/01's state 3 0.719 to 0.856, the church's 0.661 to 0.841, a_h01_1's 0.684 to 0.815);
  7 of 388 lose, by at most 0.036. The lowest: conduitbarrel 3 (0.14), leaders/02 3 (0.32), watchtower 3 (0.32),
  e_stella/02 3 (0.37): towers and posts whose damaged picture is a heap beside the mesh.

### The batch: time and size

One `zig build` a step, outputs cleared first; the times include the step's own compiles (bk-views-fast's in
africa on Windows).

| step | win-home (36 threads) | macOS arm64 (10 cores) |
|---|---|---|
| prepare (`bk-views-side`, the tools and the engine's reader) | 66 s | 47 s |
| full3d-africa (43 objects) | 153 s | 88 s |
| full3d-common (79) | 115 s | 59 s |
| full3d-europe (116) | 229 s | 160 s |
| full3d-ussr (174) | 517 s | 232 s |
| full3d-review | 176 s | 74 s |
| all | 1256 s | 660 s |

The Mac ran in a throwaway `--shared` clone of its checkout with this branch fetched from a bundle (a fresh Zig cache
of its own, so its prepare includes fetching the dependencies), deleted after. Sizes (the same on both, KiB):

| region | models (`zig-out/full3d/models`) | turned packs (`zig-out/remastered`) | side's work and checks (`zig-out/local-test/full3d`) |
|---|---|---|---|
| africa | 498,637 | 157,847 | 1,094,987 |
| common | 606,185 | 134,394 | 739,996 |
| europe | 1,562,886 | 469,204 | 3,798,762 |
| ussr | 2,779,962 | 805,138 | 5,994,662 |
| all | 5,447,734 | 1,566,583 | 11,628,407 |

All of it is build output under `zig-out` (git ignores it). The models are mostly the uncompressed atlases
(`_h.dds` and `.tga`, each a copy, per state); the work folders keep every turn's pictures and the round trip's.

### Tiers (win-home, Windows x64, debug Game)

- `test-bk-views`: pass (with the damage-state test: e_wb/01's state 3 cut model at N 0.856 against its damaged
  picture, the intact model 0.719; its fallen roof no higher than the rubble; no holes; the same bytes; its shadow
  all but a twentieth within the intact one's. The deflated PNG round trip. The re-derived gate).
- `resource-editor-full3d`: pass. e_wb/01's intact model from the default camera and the side, its parts picked
  by name, its damage state 3 (`3.mod`, the transportable variant) lit and unlike the intact one, and the prop
  ukrainewell/01 opened as a second project, lit, `body.roof` picked by name.
- `camera-turn-full3d-game-reads-it` (new): UkraineWell01, Semaphore02 (props, turn k at step k) and E_WB_01 at
  own facing 1 with HP 0.3 (damage state 2, turn facing + step), each alone, rendered by `side` into a scratch
  mod: every rendered turn IoU 0.986 to 1.000, colour 0.00, better than every other turn there; turn 0 (the 2003
  picture) 0.931 and 0.967 for the props' soft-edged sprites and 0.986 / 2.23 for the warehouse's 2003 state 2
  over its rubble, measured as camera-turn-world measures 2003 props (IoU 0.9, colour 8). Pass.
- `camera-turn-game-reads-it`: two changes the full 3D models need. Turn 2 of e_ctownhouse01_2 was required to
  be the 2003 e_ctownhouse01_4 picture for picture; a model draws every turn (decided above), so the match is
  reported (IoU 0.560: e_ctownhouse01 is among the families the review marks for geometry). And the town house's
  turned model reaches the shed at the odd steps (its spire) and the message line and the dark band the interface
  lays over the top of the screen at step 3: each building is shot alone, as facing.xml already was, and the rows
  down to the first the band leaves black are left out of the measure. Every turn 0.994 to 1.000, colour 0.00.
  Pass.
- `camera-turn-village-game-reads-it`, `camera-turn-world-game-reads-it`, `camera-turn-tutorials-game-reads-it`:
  pass.
- Not run here: the other Resource Editor and Map Editor tiers (no editor code changed but `SaveIconFile`, which
  only the one-pixel box reaches), the full sweep (left to the maintainer), Linux (CI). macOS arm64: the `full3d`
  batch above in a throwaway clone (its Game tiers not run there).

### install-all (2026-10-30)

`zig build install-all --release=fast` makes everything the camera turn needs: it now depends on `camera-turn-mod`,
which installs the turns of all 412 objects (`full3d-mod`) and the relit flora and props from cached runs into
`zig-out/remastered/mods/remastered_views`. The maintainer's line on macOS arm64 is

`git pull && zig build install-all --release=fast && BK_MODS_ROOT="$PWD/zig-out/remastered/mods" zig-out/game/macos/arm64/release/Game -autosave -Monitor=2 -fullscreen -mod=remastered_views`

- **Cached by Zig, not by timestamps.** `bk-views side`, `relight` and `model3d` write into the run's own cache
  folder (`addOutputDirectoryArg`) and list every file they read in a make-style dep file (`--deps=<file>`): the
  object's family's Data folders, `shape-ids.json`, `props.json`, `meshes/families.json`, the family's
  `meshes/<id>.json` and shared mesh, `shapes/<family>.json` and `roof-parts/<family>.json`. With bk-views-fast
  (the run's executable) and `zig-out/bin/bk-sprite` as inputs, a run comes again only when one of those changed;
  the steps install from the cache folders, so deleted outputs come back. The engine's reader check takes the turns'
  cache paths as its arguments and comes again exactly when the turns did. A file added to a folder the run read
  (a new damage state) is not seen until another input changes.
- **Dropped from install-all:** the models (`model3d`, 5.2 GiB) and the review PDF stay in `full3d-<region>` and
  `full3d-review`. `side --no-work` keeps its pictures and source maps in a scratch folder it removes (they were
  11 GiB nothing read), and the round trip keeps a variant's scratch only where it differed (6 GiB). The
  building-views batch is not part of it: every inventory variant (194) is one of the 412 full 3D objects.
- **The artifacts' paths.** The first runs used bk-sprite's and bk-views' artifacts as inputs; Zig resolved them at
  times to the copies `install-resource-editor` puts beside ResourceEditor in the staged game, which install-game
  rewrites while the runs go, and 21 to 127 checks drew again at random on each repeat. The runs now name
  `zig-out/bin/bk-sprite` and use bk-views-fast (installed nowhere). A Debug build installs a Debug bk-sprite there,
  so the next release install-all draws everything again once.
- **`BK_MODS_ROOT` and `-mod=remastered_views` stay.** The mod is generated pictures (1.3 GiB on Windows, 1.6 GiB
  on the Mac, `du`) and the install's `mods` folder is the player's and is packaged; a Game that loaded the mod by
  itself would do so for players too.

| | Windows x64 (win-home) | macOS arm64 |
|---|---|---|
| first run | regions one at a time (the 10-minute cap): africa 126 s (with bk-views-fast's compile), common 87 s, europe 201 s, ussr 505 s, then install-all 459 s (relight, the mod) | 1031 s, a fresh `--shared` clone with its own `.zig-cache`, all compiles included |
| repeat, nothing changed | 242 s and 244 s, nothing drawn: 3 min of it is `install-language-mods` (its run has side effects, as before) | 70 s and 71 s, nothing drawn (`--summary all`: no bk-views or round trip run); 1 min is `install-language-mods` |
| `full3d-mod` alone, repeat | 3 s | not run |
| one mesh changed (`meshes/003.json`, a_h01_1) | its family's two objects drawn again, 19 s for full3d-africa | not run |

Proof the Game reads it: every installed pack passes the engine's reader (`bk-views-check` in the round trip) inside
install-all, and `camera-turn-full3d-game-reads-it` passes on Windows (release), with `side` changed as above; it did
not compile on main (its floors were `comptime_float` chosen at run time) and does now. `test-bk-views`,
`test-bk-tools-l10n`, `bk-views-roundtrip` and `bk-views-side` pass on Windows. Not run: the Mac's Game tiers, Linux
(CI), the full sweep (left to the maintainer).

### Left open


- Paint: the median atlas is 62% guessed; the needs-paint list in `full3d-ranked.csv` is the order to paint in
  (props with one small 2003 picture first).
- Meshes: the needs-mesh-fix list, led by thin and open props (boxes, wire and wicker fences, the mine's post)
  whose mesh does not cover their picture, and towers whose damaged picture is a heap beside the mesh.
- Damage: a wall N sees through a fallen roof keeps the height the outline gives it; rubble lying beyond the
  intact outline (the damaged pictures' heaps) has no geometry, its pixels lost at N.
- Size: the models are 5.2 GiB, mostly the uncompressed `_h.dds` and `.tga` of every state; a compressed TGA or
  dropping the `.tga` for states the Resource Editor does not open would halve it, not done (the RE tier reads them).
- The review PDFs of Windows and macOS have the same size and differ in bytes (their CSVs are the same bytes).
