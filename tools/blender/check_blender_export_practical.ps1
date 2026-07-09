$ErrorActionPreference = "Stop"

$root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$converter = Join-Path $root "Sources\src\BZMConvertor\Debug\BZMConvertor.exe"
if (-not (Test-Path $converter)) {
    throw "BZMConvertor.exe was not found at $converter. Build /t:BZMConvertor first."
}

$converterDir = Split-Path -Parent $converter
$configuration = Split-Path -Leaf $converterDir
$streamio = Join-Path $root "Sources\src\StreamIO\$configuration\StreamIO.dll"
if (Test-Path $streamio) {
    Copy-Item -LiteralPath $streamio -Destination (Join-Path $converterDir "StreamIO.dll") -Force
}
else {
    throw "StreamIO.dll was not found at $streamio. Build /t:StreamIO first."
}

$blender = Get-Command blender -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty Source
if (-not $blender) {
    $blender = Get-ChildItem -Path "$env:ProgramFiles\Blender Foundation" -Recurse -Filter blender.exe -ErrorAction SilentlyContinue |
        Select-Object -First 1 -ExpandProperty FullName
}
if (-not $blender) {
    throw "Blender executable was not found. Install Blender or add blender.exe to PATH."
}

$temp = Join-Path $root "Sources\src\BZMConvertor\Debug\blender_export_practical"
if (Test-Path $temp) {
    Remove-Item -LiteralPath $temp -Recurse -Force
}
New-Item -ItemType Directory -Path $temp | Out-Null

$obj = Join-Path $temp "triangle.obj"
$skeleton = Join-Path $temp "triangle.skeleton.txt"
$animation = Join-Path $temp "triangle.anim.txt"
$mod = Join-Path $temp "triangle.mod"
$blenderScript = Join-Path $temp "run_blender_export.py"

@"
import importlib.util
import pathlib

import bpy

exporter_path = pathlib.Path(r"$PSScriptRoot") / "blitzkrieg_export.py"
out_path = pathlib.Path(r"$mod")
converter_path = pathlib.Path(r"$converter")

spec = importlib.util.spec_from_file_location("blitzkrieg_export", exporter_path)
exporter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(exporter)

bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete()

mesh = bpy.data.meshes.new("TriangleMesh")
mesh.from_pydata([(0, 0, 0), (1, 0, 0), (0, 1, 0)], [], [(0, 1, 2)])
mesh.update()
mesh_obj = bpy.data.objects.new("Triangle", mesh)
bpy.context.collection.objects.link(mesh_obj)

bpy.ops.object.armature_add(location=(0, 0, 0))
armature = bpy.context.object
armature.name = "TriangleArmature"
armature.data.name = "TriangleSkeleton"
armature.data.bones[0].name = "Root"

bpy.ops.object.mode_set(mode="POSE")
pose_bone = armature.pose.bones["Root"]
pose_bone.location = (0, 0, 0)
pose_bone.keyframe_insert(data_path="location", frame=0)
pose_bone.location = (1, 0, 0)
pose_bone.keyframe_insert(data_path="location", frame=1)
bpy.ops.object.mode_set(mode="OBJECT")

bpy.ops.object.select_all(action="DESELECT")
mesh_obj.select_set(True)
armature.select_set(True)
bpy.context.view_layer.objects.active = armature

obj_path = out_path.with_suffix(".obj")
skeleton_path = out_path.with_suffix(".skeleton.txt")
animation_path = out_path.with_suffix(".anim.txt")

exporter.write_obj(obj_path, bpy.context)
exporter.write_skeleton(skeleton_path, bpy.context)
if not exporter.write_animations(animation_path, bpy.context):
    raise RuntimeError("Expected Blender animation sidecar was not written")

result = exporter.run_converter(converter_path, obj_path, out_path, skeleton_path, animation_path)
if result.returncode != 0:
    raise RuntimeError("BZMConvertor failed with exit code %d" % result.returncode)
"@ | Set-Content -LiteralPath $blenderScript -Encoding UTF8

& $blender --background --factory-startup --python $blenderScript
if ($LASTEXITCODE -ne 0) { throw "Blender export test failed with exit code $LASTEXITCODE" }

if (-not (Test-Path $mod)) {
    throw "Expected Blender-generated MOD was not created: $mod"
}

@"
# Minimal file shaped like the Blender exporter OBJ output.
o Triangle
v 0 0 0
v 1 0 0
v 0 1 0
vn 0 0 1
vn 0 0 1
vn 0 0 1
vt 0 0
vt 1 0
vt 0 1
f 1/1/1 2/2/2 3/3/3
"@ | Set-Content -LiteralPath $obj -Encoding ASCII

@"
node 0 -1 Root 0 0 0 0 0 0 1 0
"@ | Set-Content -LiteralPath $skeleton -Encoding ASCII

@"
anim Move 2 0 -1 -1
key 0 0 0 0 0 0 0 0 1
key 0 1 0 0 0 0 0 0 1
"@ | Set-Content -LiteralPath $animation -Encoding ASCII

Push-Location (Split-Path -Parent $converter)
try {
    & $converter -validateobj2mod $obj $skeleton $animation
    if ($LASTEXITCODE -ne 0) { throw "BZMConvertor validation failed with exit code $LASTEXITCODE" }

    & $converter -obj2mod $obj $mod $skeleton $animation
    if ($LASTEXITCODE -ne 0) { throw "BZMConvertor conversion failed with exit code $LASTEXITCODE" }
}
finally {
    Pop-Location
}

if (-not (Test-Path $mod)) {
    throw "Expected MOD was not created: $mod"
}

Write-Host "Blender exporter practical conversion passed: $mod"
