# Modelling an object by hand from its 2003 picture

How to turn a building or prop into a real 3D model with a texture on each flat face, so that every camera turn follows
from the shape. It extends [full 3D objects](2026-10-29-full-3d-objects-design.md): that design gets its geometry from the
tracked review meshes and paints them from the picture, which leaves guessed pixels and details that fall apart when turned.

The maintainer (2026-11-02): "Why can't I have a wire model and put the textures on the flat sides. The roof for example has
two windows under less strongly sloped roofs. For a correct 3D model you need to model this in 3D."

**Status.** Proved on one house, `e_house01_1`, in a preview renderer, and judged by the maintainer over five review rounds. It
has not been through `bk_views` or the Game, and damage and winter states are not done. Advice for props and other kinds of
building below is expected to hold but is untested; change this file when a real case says otherwise.

**Worked example.** `tools/house_model/e_house01_1.py` is the whole house in about 500 lines, with its camera (`rast.py`), sprite
reader (`bk1.py`) and shadows (`castshadow.py`). Run it from the checkout root; it reads `Data/` and writes only to
`zig-out/local-test/house_model/`:

    python -I tools/house_model/e_house01_1.py

It needs numpy and Pillow. 102 triangles, 46 flat faces, one 1024 x 552 atlas.

## Rules

1. **BK1 art only.** No Blitzkrieg 2 file, and nothing uploaded to an outside service, without the maintainer's say and an answer
   from Nival.
2. **Measure, do not assume.** Every dimension comes from the picture's pixels. The house was not the shape it looked like.
3. **What sticks out is geometry.** A dormer, a porch, a chimney or a roof overhang is modelled. Paint is only for what is
   flat on its face: a window in a wall, a skylight in a roof.
4. **Never stretch a texel and never invent one.** A seen face keeps its own pixels. A hidden face reuses art of a seen face at
   the same scale.
5. **Light belongs to the world, not to the texture.** Shadows the 2003 artist painted are removed and computed.
6. **The picture is a guide for the side it shows.** When a turn looks wrong, the turn wins over the picture.
7. **The maintainer judges by eye.** Numbers below help you find faults; they do not close a review.

## The camera

World units, z up, one pixel per unit, the zero cross at the object's origin:

    sx = (x + y) * 0.7071
    sy = (x - y) * 0.3536 - z * 0.8660

A step along x moves right and down, a step along y right and up. So two points' `sx` difference gives their distance along
an axis (`dsx / 0.7071`), and once x and y are known, `sy` gives z.

| Turn | The camera sees faces towards |
|---|---|
| 0 | +x and -y, the side the 2003 picture shows |
| 1 | -x and -y |
| 2 | -x and +y |
| 3 | +x and +y |

Each face is therefore seen in exactly two turns. Faces towards +y or -x are never in the picture.

The sun does not turn with the object: its direction stays (1, 1, -2) on the rotated model.

## Steps

### 1. Collect what exists

For the variant, read `1` (the picture), `1s` (its shadow), `2` and `3` (damage), `1.xml` (passability and origin). Lay out the
family and its relatives on one sheet: `e_house01_2` showed the whole chimney that `e_house01_1` hides.

### 2. Measure on a grid

Draw the picture at 6x with a labelled grid in zero-cross pixels (`sx`, `sy`). Read the corners: ridge ends, eave tips, wall
corners at the ground and at the top, the porch. Solve them with the camera above. Check each with a point you did not use:
the house's points agreed within a pixel or two.

Then overlay the model's edges on the picture at 10x and correct it. This found what reading corners did not:

- **The roof is asymmetric.** From the ridge, 71 units at 45° to the front and 38 units at 60° to the back.
- **The dormers flare.** 15 units wide at the window and 27 where they meet the roof, with their own flatter roof. A dark wedge
  on the picture that looked like shadow was the dormer's side wall.
- **The chimney stands behind the ridge.** That is why the picture shows only its tip: its corners placed there land on the
  tip's corners within a pixel. No part of it shows on the front slope, which rules out anywhere else.
- **The footprint is a cross-check.** The walls measured 92 x 89.5 against the 2003 passability's 4 x 4 cells of 22.63 units.

### 3. Write the faces

One `Face` per flat surface: a normal, its polygons, its own texture rectangle. A surface with something standing in it gets real
holes (the front slope around the dormers). Slabs get their edge faces (verges, eaves). Add undersides: the camera sees under a
steep overhang, and without them a roof edge shows as a thin line in the air. Keep triangles low: this house is 102.

### 4. Texture what the picture shows

For each texel of a visible face, project it to the picture and take that pixel, divided by the light the picture has for that
normal. Take it only where a first render says this face is what the pixel shows, or a post will print on the wall behind it.

Read the picture with the geometry that matches it. If the built model departs from the picture (step 7), keep the picture's
geometry for this step (`polys_pic`, `pic_shift`).

Small gaps (behind a post, at an edge) take the nearest seen texel of the same face. Check each face's seen share: a large face that
shows little of itself is probably misplaced. Thin faces and dormer sides are low anyway.

### 5. Reuse art for hidden faces

Each hidden face names a seen source and a mapping, at the same scale:

| Hidden | Source |
|---|---|
| Back slope, parts of the slope behind dormers | A long clean strip of the seen slope, mirrored tile |
| Far gable | Near gable; mirrored past anything only the near end has |
| Back wall | Seen long wall up to its height; timber band of the gable above it |
| Far verge, back eave | Near verge, front eave |
| The far side of a dormer, post or chimney | Its near side |
| The part of a detail no picture shows | A relative that shows it, toned to the object's own visible part |
| Undersides | One flat colour, the darkest timber |

Mirror rather than repeat, so tiles have no seam, and choose a clean source: look at the face rectified with a grid first. A source
beside a bright roof edge carries that light with it; a strip with a painted shadow repeats it as dark dashes.

### 6. Remove painted light and compute it

Find what the artist painted for one sun and replace it with neighbouring art of the same material:

- The dark band on a gable under the overhang. Turned, it looks like a hole in the roof.
- The shadow beside a dormer, on the slope.
- Any black in timber, and corners of plaster painted in shade.

Then compute shadows per turn from the model (`castshadow.py`): a ray to the sun from each surface sample. The overhang shadows
the wall, the dormers and chimney shadow the roof, and the roof shadows the porch. A lit surface in shadow keeps 60 % of its
light. Full sun contrast burns painted art (a roof turned to the sun reached 1.6x), so this house lights with ambient 0.8 and
diffuse 0.35, about 1.2x.

### 7. Correct the proportions by the turns

A picture can hide proportions that look wrong from elsewhere. These took the review rounds on the house:

| Seen at | Looked like | Cause | Fix |
|---|---|---|---|
| Turns 1, 3 | Roof slid back, body too short | Roof ran 18.5 past the wall at one end and 3 at the other | Longer body: 89.5 to 100 |
| Turns 1, 2 | Leak in the roof | Painted band on the gable, near end too short | Remove the paint; near overhang 3 to 10 |
| Turns 2, 3 | Overhang too short, then too long | Far overhang 8, then 5 | 7 |
| Turn 0 | Second gable window hidden | Near overhang 12 | 10 is the limit |

Overhang moves things at a known rate: a roof edge pushed out by a unit moves on screen by 0.71 across and 0.35 down, and it
covers whatever wall lies behind that line. Work out what it will hide before raising it. Note where the body passes the 2003
passability: this one is about half a tile over at the far end, unsolved until the Game shows it.

### 8. Review

Make a small PDF in the review folder and look at it yourself first. Page one: the picture beside all four turns at 2x, on neutral
grass, with the ground shadow. Then a before and after for each thing reported, then close-ups, then the wire model and atlas.
Fix only what was reported and keep what was approved. Zoom in on each turn before sending; several faults here were found only at
4x.

## Checklist before it goes to the maintainer

- Every visible face at each of the four turns, zoomed: no stray lines, no holes, no see-through undersides.
- No painted shadow left: gables, dormers, corners, black in timber.
- No tile seams or repeating dashes on roofs.
- Details are solid at all four turns and the chimney stands on something.
- Overhangs look alike around the object, and nothing important is hidden by them.
- Report the measures: triangles, outline against the picture at turn 0, and the walls against the passability. Say what was reused.

## For the other 411 objects

The order matters, because relatives share parts. Take one object from each family first, then reuse its faces, chimney and dormer
definitions for the rest of that family, changing only what the pictures show. `e_house01_3` is the next test of that saving: by eye it shares
this house's porch and roof, with larger dormers. `e_house01_2` has no porch and shares less. Measure how long a relative takes before planning the whole set.

Expected to hold for props, unproven: a low object needs far fewer faces, and rule 3 still decides what is modelled.
Something thin and cut out by alpha (a fence, a sign) may stay one flat face with its picture on both sides.

## Still to do

- Through `bk_views` into the Game. The `.obj` beside the renders has not been checked by `bk-map validate-obj2mod`.
- Using the model at turn 0 as well, as the maintainer's computed-shadow request implies, in place of the 2003 picture. Turn 0 then
  loses the picture's sharpness, and the outline overlap drops to 0.94 on this house because its body and roof are deliberately larger.
- Damage (`2`, `3`) and winter (`1w`) states, and the passability question above.
- The ground shadow is larger than the 2003 one, since it follows the real roof.
- The back of this house has no door and its windows repeat, by rule 4. The maintainer accepted this for now.
