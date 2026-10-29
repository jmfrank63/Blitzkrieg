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
  and from the intact atlas elsewhere, its guessed paint marked as here.
- **Inspection: the Resource Editor's Unit editor**, no new viewer. The `.mod` and the summer atlas in place of a
  unit fixture's models and pictures: the preview shows the model lit from the 2003 camera and from above, and its
  parts are the skeleton's nodes, picked by name (`resource-editor-full3d`).
- **Which objects:** `tools/zig/fixtures/bk-views/full3d.json` lists the full 3D objects by number; `side` draws
  every other object as before. The list grows to all 412 and then goes away.

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
