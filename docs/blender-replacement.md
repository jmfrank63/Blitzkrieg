# Blender Replacement Pipeline (No Maya40)

This repository contains a native C++ conversion and validation path that replaces the Maya40 exporter workflow for Blender-authored assets.

The workflow is based on `BZMConvertor.exe` and supports:

- OBJ to MOD conversion
- Optional skeleton sidecar import
- Optional animation sidecar import
- Validation-only mode for CI or preflight checks

## Tool Location

Built executable path:

- `Sources/src/BZMConvertor/Debug/BZMConvertor.exe`

If you build from VS Code tasks, run:

- `Build All (Debug)` or a targeted BZMConvertor project build.

## Command Reference

Use BZMConvertor with -obj2mod:

```powershell
BZMConvertor.exe -obj2mod <input.obj> <output.mod>
```

Optional rig/animation sidecars:

```powershell
BZMConvertor.exe -obj2mod <input.obj> <output.mod> <skeleton.txt> <animation.txt>
```

Validate only (no MOD write):

```powershell
BZMConvertor.exe -validateobj2mod <input.obj> [skeleton.txt] [animation.txt]
```

## Conversion Examples

Mesh only:

```powershell
BZMConvertor.exe -obj2mod Data\Units\MyUnit\model.obj Data\Units\MyUnit\1.mod
```

Mesh and rig:

```powershell
BZMConvertor.exe -obj2mod Data\Units\MyUnit\model.obj Data\Units\MyUnit\1.mod Data\Units\MyUnit\skeleton.txt
```

Mesh, rig, and animation:

```powershell
BZMConvertor.exe -obj2mod Data\Units\MyUnit\model.obj Data\Units\MyUnit\1.mod Data\Units\MyUnit\skeleton.txt Data\Units\MyUnit\anim.txt
```

## Validation Examples

Validate mesh only:

```powershell
BZMConvertor.exe -validateobj2mod Data\Units\MyUnit\model.obj
```

Validate mesh and sidecars:

```powershell
BZMConvertor.exe -validateobj2mod Data\Units\MyUnit\model.obj Data\Units\MyUnit\skeleton.txt Data\Units\MyUnit\anim.txt
```

Successful validation prints a summary line similar to:

```text
Validation passed: mesh=<verts> verts, skeleton=<nodes> nodes, animations=<count>
```

## Blender Workflow

1. In Blender, export mesh as Wavefront OBJ.
2. Triangulate meshes before export (or in export settings).
3. Ensure normals/UVs are exported.
4. Run BZMConvertor -obj2mod to produce .mod.

Recommended Blender export settings:

- Geometry: Triangulate enabled
- Normals: enabled
- UVs: enabled
- Apply transforms before export if your asset pipeline expects baked transforms

## Notes

- This path is Maya SDK free.
- If sidecars are not provided, converter creates one mesh and a minimal single-root skeleton (Root).
- Face polygons are triangulated during import (fan triangulation).
- If normals or UVs are missing, defaults are used.
- Validation mode does not write output MOD files.

## Sidecar Formats

Skeleton file (`skeleton.txt`) line format:

```text
node <index> <parentIndex> <name> <boneX> <boneY> <boneZ> <qx> <qy> <qz> <qw> <locatorFlag>
```

Example:

```text
node 0 -1 Root 0 0 0 0 0 0 1 0
node 1 0 Turret 0 1 0 0 0 0 1 0
node 2 1 Muzzle 0 0.5 0 0 0 0 1 1
```

Animation file (`animation.txt`) line formats:

```text
anim <name> <numKeys> <actionKey> <aabbAIndex> <aabbDIndex>
key <nodeIndex> <keyIndex> <posX> <posY> <posZ> <qx> <qy> <qz> <qw>
```

Example:

```text
anim idle 2 0 -1 -1
key 0 0 0 0 0 0 0 0 1
key 0 1 0 0 0 0 0 0 1
key 1 0 0 1 0 0 0 0 1
key 1 1 0 1 0 0 0 0 1
```

Notes:
- `parentIndex = -1` marks top node.
- `locatorFlag` is `0` or `1`.
- `keyIndex` is zero-based and must be `< numKeys`.

## Validation Failure Guide

Common validation failures and fixes:

- "mesh index references component out of range"
	Fix: ensure OBJ faces are valid and not corrupted.

- "skeleton graph must have exactly 1 root"
	Fix: only one node may use `parentIndex = -1`.

- "cycle detected"
	Fix: parent-child relationships must form a tree (no loops).

- "node is not reachable from top node"
	Fix: connect all nodes under the root hierarchy.

- "animation ... node rows but skeleton has ..."
	Fix: animation data must match the exact skeleton node count.

- "action key ... out of range"
	Fix: action key must be in `[0, numKeys)`.

## Recommended Pipeline

1. Export `model.obj` from Blender.
2. Generate or update `skeleton.txt` and `anim.txt` sidecars.
3. Run `-validateobj2mod` first.
4. Run `-obj2mod` only after validation passes.
5. Place resulting `.mod` into the unit asset location expected by the game/editor.

## Validator Checks

The validator fails conversion with clear diagnostics when it finds:

- Empty mesh/components or non-triangle index buffer.
- Mesh index references outside component range.
- Skeleton with invalid top index, empty node names, or slot/index mismatch.
- Skeleton graph with multiple roots, cycles, duplicate parents, or unreachable nodes.
- Locator indices outside node range.
- Animations with empty names, zero keys, wrong node-row count, or action key out of range.

## Source

Implementation is in Sources/src/bzmconvertor/main.cpp.