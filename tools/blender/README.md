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
3. Set **2003 folder** to the object's `Data` folder (optional) and **Output folder**, then **Write paint templates**.

It writes `step_0.ora` to `step_3.ora`, which Krita opens with their layers, and `layout.json` with each canvas's size and
zero cross. Top to bottom: `zero cross`, `wire`, `faces` (a colour per flat face, to select by), `light` (Multiply), `paint here`,
the 2003 pictures that show this side in every state (another variant's picture is laid by its outline, so its place is an
estimate), `model texture` where the model has one, `ground shadow`. The light is the Game's: the sun fixed in the world at each
camera step. The camera and light are computed from the same formulas as `bk-views`, not by Blender's renderer, so a canvas lines up
with the Game's pictures to the pixel.

Run `python -I -B tools/blender/test_blitzkrieg_views.py` for its tests. Painted pictures do not yet have a step into the Game.
