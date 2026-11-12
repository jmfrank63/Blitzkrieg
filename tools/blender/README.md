# Blitzkrieg Blender Exporter

This folder contains the open Blender-side replacement for the removed Maya 4.0
`A7ExportModel` plugin.

The exporter intentionally uses the existing open conversion path:

1. Blender writes an intermediate OBJ file.
2. Blender writes a `*.skeleton.txt` sidecar in the format accepted by
   `bk-map obj2mod`.
3. Blender optionally writes a `*.anim.txt` sidecar for selected armature
   actions.
4. The add-on optionally runs `bk-map obj2mod` to create the final `*.mod`
   (`bk-map` is in the game's `Tools/` folder on Linux and macOS, and in
   `zig-out/bin` after `zig build bk-map` on every platform).

Install `blitzkrieg_export.py` as a Blender add-on, select the mesh objects to
export, then use `File > Export > Blitzkrieg MOD (.mod)`.

For static meshes, no armature is required; the exporter writes a single `Root`
node. If an armature is selected, its bones are exported to the skeleton sidecar
and all Blender actions are exported as animation keys.

Run `tools\blender\check_blender_exporter.ps1` for fast exporter unit tests.
Run `tools\blender\check_blender_export_practical.ps1` after `zig build bk-map`
to exercise Blender headless export and conversion to `.mod`.

## Paint templates for turned views

`blitzkrieg_views.py` is a second add-on: it writes what you need to paint an object's turned views by hand. Install it the same
way; the panel is in the 3D view's sidebar (press N) under **BK1**.

1. Load a shape: **Import hand model** for `tools/zig/fixtures/bk-views/hand-models/<name>.json`, `File > Import > Wavefront (.obj)`
   for anything else (forward Y, up Z), or model one. Keep z up, the object's zero cross at the origin, 1 Blender unit a world unit.
2. **Step 0** to **Step 3** look through the Game's camera.
3. Set **2003 folder** to the object's `Data` folder (optional), **Output folder** and **Scale**, then **Write paint templates**.

It writes `step_0.ora` to `step_3.ora`, which Krita opens with their layers, and `layout.json` with each canvas's scale, size and
zero cross. A canvas is **Scale** times the Game's size (4 by default): the wire, faces, light and shadow are computed at that size,
one pixel wide lines included, and the 2003 pictures and the model's texture are enlarged by Lanczos filtering, so they look soft and
are there to paint over. Top to bottom: `zero cross`, `wire`, `faces` (a colour per flat face, to select by), `light` (Multiply), `paint here`,
the 2003 pictures that show this side in every state (another variant's picture is laid by its outline, so its place is an
estimate), `model texture` where the model has one, `ground shadow`. The light is the Game's: the sun fixed in the world at each
camera step. The camera and light are computed from the same formulas as `bk-views`, not by Blender's renderer, so a canvas lines up
with the Game's pictures to the pixel.

Run `python -I -B tools/blender/test_blitzkrieg_views.py` for its tests. Painted or rendered pictures go into the Game as kit views
(below).

## Kit views: rendered views into the Game

The buildings' Blender kit models stay outside the repo. What the Game takes of them is their rendered views, tracked in
`tools/zig/fixtures/bk-views/kit-views/` (194 buildings, export of 2026-11-12):

- `index.json`: `[{"page": 58, "path": "europe/summer/e_house01_1"}, ...]`, `path` below `Data/Buildings`.
- `<path>/views.json`: per damage state (`1` intact, `2` damaged, `3` destroyed) and quarter turn (`0` to `3`) the picture's `size` and
  its `zero`, the pixel (x, y from the top left) of the object's zero cross. It is a ground point and may lie a little outside the
  picture, which is cut to its content.
- `<path>/<state>_<turn>.png`: 8-bit RGBA at the Game's scale (one pixel a world unit, the 2003 camera), one-bit alpha. Turn j is the
  building turned j times by (x, y) -> (-y, x) (`bk-views` `turnPoint`), turn 0 the view the 2003 picture shows. All four are lit by
  the 2003 light fixed in the world at turn 0: the pictures of camera step j for a building at facing 0.

`bk-views kit <kit-dir> <variant-dir> <turned-dir> <out-dir>` lays them over the turns `bk-views side` drew for the building: turn 0
into `<variant>` (a copy of the 2003 folder, or the hand model's) and turn j into `<variant>_r<j>` and `<variant>_r<j>_l<j>`. Each
state's picture pack is composed from the kit view by `bk-sprite compose-pack`; the shadows, ground pictures and winter pictures stay
the pictures they were (composed again on the folder's canvas, which a kit view may widen, under the picture they now lie with), as
do `1.xml`, the icon and the name. A kit view wins over the mesh render and over a hand model. There are no kit views of a winter, no
camera-lit ones and no shadows of their own: a building at another facing shows the same pictures, in winter the pictures `side`
made, and the shadow is the mesh's.

`zig build install-all` (and `camera-turn-mod`, `full3d-mod`, `kit-views` alone) runs it for every building of the index after its
`side` run, checks every pack (`bk-views-roundtrip --kit=`: the engine's reader, compose-pack again byte for byte, and the decomposed
picture against the fixture pixel for pixel at its zero cross) and installs the result in the mod
`zig-out/remastered/mods/remastered_views`. A view that cannot be composed fails the build with its path. The runs are cached per
building: a new export draws again only what changed.

To see them (the mod is kept out of the installation's `Mods` folder, which is the player's):

```
zig build run-remastered -- kit_views_01.xml
zig build run-remastered-map-editor
zig build run-remastered-resource-editor
```

or, without the build system, from PowerShell in the checkout (debug install; `release` after `--release=fast`):

```
$env:BK_MODS_ROOT = "$PWD\zig-out\remastered\mods"
& "$PWD\zig-out\game\windows\x86_64\debug\Game.exe" -mod=remastered_views kit_views_01.xml
& "$PWD\zig-out\game\windows\x86_64\debug\MapEditor.exe" -mod=remastered_views
& "$PWD\zig-out\game\windows\x86_64\debug\ResourceEditor.exe" -mod=remastered_views
```

The mod's `maps/kit_views_01.xml` to `kit_views_22.xml` stand every building of the index on a flat map, nine each in the index's
order (`kit_views_<nn>_damaged.xml`: at HP 0.3, the damaged state). In the Game switch Remastered on (Options > Settings > Gameplay)
and turn the camera with `,` and `.`; without it the Game shows turn 0. In the Map Editor open a map of the mod (or place buildings)
and turn one with Q and E. In the Resource Editor the Building editor's Sides window shows a building folder's four sides.

To take a new export: replace the folders under `tools/zig/fixtures/bk-views/kit-views/` with the export's (`index.json` and each
building's `views.json` and PNGs; Git stores the JSON files with CRLF), then run `zig build test-bk-views` (the fixtures agree with
their `views.json` and with `Data/Buildings`), `zig build install-all` and the tiers `camera-turn-kit-game-reads-it`,
`camera-turn-kit-editor-reads-it` and `resource-editor-auto-bld`. `zig build camera-turn-kit-review` shoots four of the maps at the
four camera steps into `zig-out/review/kit-views`.
