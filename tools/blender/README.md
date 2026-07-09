# Blitzkrieg Blender Exporter

This folder contains the open Blender-side replacement for the removed Maya 4.0
`A7ExportModel` plugin.

The exporter intentionally uses the existing open conversion path:

1. Blender writes an intermediate OBJ file.
2. Blender writes a `*.skeleton.txt` sidecar in the format accepted by
   `BZMConvertor -obj2mod`.
3. Blender optionally writes a `*.anim.txt` sidecar for selected armature
   actions.
4. The add-on optionally runs `BZMConvertor.exe` to create the final `*.mod`.

Install `blitzkrieg_export.py` as a Blender add-on, select the mesh objects to
export, then use `File > Export > Blitzkrieg MOD (.mod)`.

For static meshes, no armature is required; the exporter writes a single `Root`
node. If an armature is selected, its bones are exported to the skeleton sidecar
and all Blender actions are exported as animation keys.

Run `tools\blender\check_blender_exporter.ps1` for fast exporter unit tests.
Run `tools\blender\check_blender_export_practical.ps1` after building
`BZMConvertor` to exercise Blender headless export and conversion to `.mod`.
