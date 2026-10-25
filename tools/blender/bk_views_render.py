"""Blender side of `bk-views side` (tools/zig/bk_views.zig): casts the 2003
camera's parallel rays at a building's block-out and writes what they hit.

Run headless: blender -b --factory-startup --python bk_views_render.py -- <job.json>

The job (written by bk-views) lists meshes (raw f32 vertices, u32 quads) and
views. A view is an orthographic camera: the ray of pixel (x, y) starts at
origin + (x + 0.5) u + (y + 0.5) v and runs along dir. For every pixel the
script writes the hit point (three f32, NaN for a miss) to <out>.pos and one
byte of flags to <out>.flags:

- bit c: the hit point, moved by check c's matrix and offset (the identity, or
  a mirror), is what check c's eye (its dir) sees there: a ray from that eye
  meets the mesh within `tolerance` of it;
- bit 7: the ground point under the pixel (z = 0) lies in the mesh's shadow
  for the light direction `light` (towards the light).

All decisions (which picture a pixel takes, light, fill) stay in bk-views.
"""

import array
import json
import math
import sys

from mathutils import Vector
from mathutils.bvhtree import BVHTree

FAR = 4000.0


def load_mesh(mesh):
    with open(mesh["verts"], "rb") as f:
        v = array.array("f", f.read())
    with open(mesh["quads"], "rb") as f:
        q = array.array("I", f.read())
    verts = [Vector((v[i], v[i + 1], v[i + 2])) for i in range(0, len(v), 3)]
    quads = [tuple(q[i:i + 4]) for i in range(0, len(q), 4)]
    return BVHTree.FromPolygons(verts, quads, all_triangles=False)


def render(tree, view, tolerance):
    width, height = view["width"], view["height"]
    origin, u, v, d = (Vector(view[k]) for k in ("origin", "u", "v", "dir"))
    checks = [(c["matrix"], Vector(c["offset"]), Vector(c["dir"])) for c in view.get("checks", [])]
    light = Vector(view["light"]) if view.get("light") else None
    pos = array.array("f", [math.nan]) * (3 * width * height)
    flags = bytearray(width * height)
    for y in range(height):
        row = origin + (y + 0.5) * v
        for x in range(width):
            o = row + (x + 0.5) * u
            i = y * width + x
            bits = 0
            if light is not None and d.z < 0:
                ground = o + (-o.z / d.z) * d
                if tree.ray_cast(ground + 0.01 * light, light)[0] is not None:
                    bits |= 128
            hit = tree.ray_cast(o, d)[0]
            if hit is not None:
                pos[3 * i:3 * i + 3] = array.array("f", hit)
                for c, (m, offset, eye) in enumerate(checks):
                    q = Vector((
                        m[0] * hit.x + m[1] * hit.y + m[2] * hit.z,
                        m[3] * hit.x + m[4] * hit.y + m[5] * hit.z,
                        m[6] * hit.x + m[7] * hit.y + m[8] * hit.z,
                    )) + offset
                    seen = tree.ray_cast(q - FAR * eye, eye)[0]
                    if seen is not None and (seen - q).length < tolerance:
                        bits |= 1 << c
            flags[i] = bits
    with open(view["out"] + ".pos", "wb") as f:
        pos.tofile(f)
    with open(view["out"] + ".flags", "wb") as f:
        f.write(flags)


def main():
    with open(sys.argv[sys.argv.index("--") + 1]) as f:
        job = json.load(f)
    trees = [load_mesh(mesh) for mesh in job["meshes"]]
    for view in job["views"]:
        render(trees[view["mesh"]], view, job["tolerance"])


main()
