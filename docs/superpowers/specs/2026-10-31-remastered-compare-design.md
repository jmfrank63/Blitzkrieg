# Resource Editor: Remastered compare view — design

Date: 2026-10-31
Status: approved in conversation, pending implementation plan
Related: docs/superpowers/specs/2026-10-29-full-3d-objects-design.md, docs/superpowers/specs/2026-10-26-camera-turn-design.md

## Problem

The Remastered views mod renders turn pictures and shadow packs offline from the Blender
models (tools/zig/bk_views*.zig), and install-all stages them into
`remastered/mods/remastered_views`. On the maintainer's Mac the textures of the full-3D
views look wrong, and an in-game screenshot cannot say precisely where the wrongness
sits: world lighting, fog and the current camera step distort everything. What is
missing is a pixel-honest way to put the 2003 original and the generated view next to
each other for one object, state and camera step — and a way to try a mirror fix without
painting by hand.

Decision of 2026-10-31: the compare lives in the Resource Editor (not Blender, not a
separate tool), as a toggle in the Buildings editor's image frame, side by side. The
first build also carries a per-object mirror flag in the views generator, and — decided
when reviewing this spec — the baked shadow packs are removed: full-3D buildings cast
their shadows live from the 3D model, exactly the way vehicles do.

## The compare view

In the Buildings editor, with a building selected, the image frame gains a
"Compare Remastered" toggle. With it on, the frame splits into two panes that share one
zoom and pan. The editor's state selector — a building's states: intact, damaged,
destroyed, whatever the object declares — drives both panes:

- Left pane: the 2003 original picture of the selected state, as the Game has always
  read it, with its anchor cross.
- Right pane: the Remastered pack's picture of the same state at camera step 0, with its
  anchor cross drawn at the same image coordinates.
- Below the panes: a strip of thumbnails of the Remastered-only views of this object —
  camera steps 1-3 and the lit variants (1l1, 2l2, 3l3). Clicking a thumbnail puts that
  picture into the right pane. The 2003 game never had turned views, so step 0 is the
  only honest side-by-side; the strip is the audit of everything else.

A flip and a 180-degree-rotate control transform the right pane's display (display only,
never stored): the anchor cross is mirrored or rotated with the pixels, the left pane
keeps the 2003 original upright. This is how the maintainer judges whether a mirrored
render would fit a model before applying it for real.

Both panes read through the engine's own readers via the EditorBridge: what is on screen
is literally what the Game reads.

## Reading the Remastered packs

The Resource Editor is installed beside the Game, so the mod folder is found relative to
the editor executable's own location (`remastered/mods/remastered_views/data/...`),
resolved with the same case-insensitive path helper the Map Editor uses for Data. The
selected building's description path (for example `e_wb/01`) names the pack folder under
the mod. If the object has no Remastered pack — most buildings do not, only the full-3D
set does — the toggle is disabled with a one-line reason. The compare writes nothing; it
is read-only.

The same toggle follows in the Objects editor for props if a prop turns out to need it;
buildings first, because that is where the wrong textures were reported.

## Shadows precomputed at load, from the 3D model

The pre-rendered shadow packs go away. Since every building and prop will be a 3D
object, their shadows are calculated from the 3D model the way vehicle shadows are:
the mesh flattened along the world's sun, at the same 50% density. Unlike a vehicle,
a building never moves, so the flattening is not repeated per frame: at mission load
every full-3D building's shadow is flattened once and kept as static geometry, and the
frames only draw it. A building's shadow is flattened once more whenever its state
changes: every damage step takes the new state's mesh, destruction takes the rubble's,
and destroyed buildings stay destroyed — the original game's building repair is not
carried over. Bridges are the exception: they can be repaired, much more slowly than
in the original, so a bridge's shadow is flattened again when its repair completes.
A pontoon bridge an engineer lays is a new 3D object mid-mission; its shadow is
flattened when it is placed, and the crossover it introduces needs nothing else —
its views come from the same packs as every other instance.
Each flatten is one small mesh; nothing else is touched.
Objects without a 3D model keep their 2003 painted shadow, unchanged.

What this takes:

- The buildings' meshes ship in the Remastered mod (today they are only inputs of
  bk_views) and the engine loads them for MOBuilding the way it loads them for units.
- MOBuilding holds its flattened shadow as static geometry, flattened at load and on
  each state change, where today it points at a shadow sprite pack; the special cases
  around baked packs (step 0 keeping the 2003 shadow, the "as it lies" paths) fall
  away with them.
- bk_views stops writing shadow packs; install-all renders pictures only and gets
  correspondingly faster.

Because the shadow is flattened from the same model the picture was rendered from,
picture and shadow can no longer drift apart, and the shadow is correct at every
facing, not only at the four camera steps. The frame cost stays that of drawing one
static dark shape per building — the pack path's own cost — because the flattening
runs once at load, not per frame. Two gates keep it honest, both on captured frames:
a dense map's mission-load time with the precompute grows by a small measured budget,
and its frame time with Remastered on stays level with Remastered off. Edge look does
not change: today's baked shadows use the same flattening, so only when the shadow is
computed differs.

## The mirror flag in the generator

The generator renders every state the object declares — intact, damaged, destroyed —
each from its own 3D model state, into the per-state folder the engine reads (the state
number is part of the pack path UpdateModelWithHP builds). Where the Blender project
has no model variant for a state yet, that state keeps its 2003 pictures until the
model exists.

bk_views gains a per-object mirror option, held in tracked data next to the generator as
a list (one line per object: description path, and the camera steps to mirror; empty by
default). A mirrored step is written into its pack flipped horizontally, with the anchor
cross mirrored with the pixels (new x = width - 1 - old x), and the lit variants treated
like their plain steps. With shadows live from the mesh, a mirrored picture needs no
mirrored shadow: the shadow stays the true mesh's, whatever the picture says.

Mirroring mirrors the baked sun shading; on a near-symmetric building that is invisible,
on an asymmetric one it is wrong. That is why the flag is per object and per step,
decided in the compare view, never a global default. The compare view is where the
decision is made: flip the right pane in the viewer, and when it fits, add the object
to the list and re-run install-all — the right pane then shows exactly the flip that
was judged, so what you previewed is what ships.

## Testing

- The editor's scenario check renders the compare frame for a tracked fixture building
  with a small tracked fixture pack (tests use only tracked data, never the user's
  mod): it measures both panes drew (pixel counts per half) and that the anchor crosses
  of left and right sit at the same image coordinates — measured pixels, never
  font-dependent text. The state selector is exercised on a fixture with an intact and
  a damaged state.
- The flip/rotate display transform is measured the same way: flipping the frame swaps
  the expected pixel counts; the anchor cross lands at the mirrored position.
- The generator flag is covered by the bk-views tests: a mirrored expectation pack,
  picture and anchor compared byte for byte, and no shadow pack written any more.
- The live shadows are covered twice, both on captured frames, never by eyeballing: the
  existing Game tier that checks shadows verifies a full-3D building's flattened mesh
  shadow lies against its footprint along the sun at all four camera steps; and the
  dense-map frame-time tier holds the performance budget from the shadows section.
- The Resource Editor's UI changed, so resource-editor-auto-bld and the editor tiers run
  before this completes; macOS and Windows builds are CI's, not claimed from here.

## Out of scope

Hand-painting pictures, a live 3D orbit view of the model, any writing to the mod from
the editor, and 90-degree rotations (they do not map onto the four camera angles).
Engineer-laid pontoon bridges and their crossovers are a feature of their own; this
design only reserves the flatten-on-placement hook their shadows need.
