"""Paint templates for the turned views of a Blitzkrieg object, from a 3D model in Blender.

For each camera step it writes a layered OpenRaster file for Krita or GIMP: wire lines, a colour per flat face, the computed
light and shadows, the ground shadow, the zero cross, what a 3D model's texture would look like, and the 2003 pictures that
show that side, on a canvas that lines up with the Game to the pixel. The camera and the sun are the formulas of
tools/zig/bk_views_mesh.zig, not Blender's renderer, so no render setting can move a pixel.

Install as a Blender add-on; the panel is in the 3D view's sidebar under "BK1". Or headless:
    blender -b --python tools/blender/blitzkrieg_views.py -- --hand e_house01_1 --data Data/Buildings/europe/summer/e_house01_1 --out <folder>
"""
import io, json, math, os, sys, zipfile, zlib, struct
import numpy as np

bl_info = {"name": "Blitzkrieg views", "blender": (4, 0, 0), "category": "Import-Export", "description": "BK1 camera and paint templates for turned views"}

R0 = np.array([0.7071067812, 0.7071067812, 0.0])
R1 = np.array([0.3535533906, -0.3535533906, -0.8660254038])
EYE = np.array([0.6123724357, -0.6123724357, 0.5])
SUN = np.array([-1.0, -1.0, 2.0]) / math.sqrt(6.0)  # towards the sun, Data/consts.xml Direction (1, 1, -2)
AMB, DIF, DENSITY = 0.8, 0.35, 0.6  # as bk_views_model.zig lights a hand model
PEAK = AMB + DIF  # the light layer tops out at white, the texture layer is raised by as much, so together they are the Game's
MARGIN = 4


def quarter(p, k):
    """The Game's camera step: the world looks turned by k quarter turns of (x, y) -> (-y, x)."""
    q = p.copy()
    x, y = p[..., 0], p[..., 1]
    for _ in range(k % 4):
        x, y = -y, x
    q[..., 0], q[..., 1] = x, y
    return q


def project(p):
    return np.stack([p @ R0, p @ R1], -1)


def flatten(p):
    """Points moved along the sun onto the ground, as the Game flattens a vehicle's mesh for its shadow."""
    q = p.copy()
    t = p[..., 2] / SUN[2]
    q[..., 0] -= SUN[0] * t
    q[..., 1] -= SUN[1] * t
    q[..., 2] = 0
    return q


def canvas(tri, k):
    """x0, y0, w, h of a view that holds the object and its shadow: a pixel (x, y) is screen point (x + x0, y + y0)."""
    s = np.concatenate([project(quarter(tri, k)).reshape(-1, 2), project(quarter(flatten(tri), k)).reshape(-1, 2)])
    lo, hi = np.floor(s.min(0)) - MARGIN, np.ceil(s.max(0)) + MARGIN
    return int(lo[0]), int(lo[1]), int(hi[0] - lo[0]), int(hi[1] - lo[1])


def raster(tri, k, box, ss, flat=False):
    """The triangle in front per sample (-1 where none) and its barycentrics; `flat`: the shadow on the ground."""
    x0, y0, W, H = box
    w, h = W * ss, H * ss
    p = quarter(flatten(tri) if flat else tri, k)
    s = (project(p) - [x0, y0]) * ss
    dep = p @ EYE
    zbuf = np.full((h, w), -np.inf)
    tid = np.full((h, w), -1, int)
    bary = np.zeros((h, w, 3))
    for i in range(len(tri)):
        (ax, ay), (bx, by), (cx, cy) = s[i]
        area = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax)
        if abs(area) < 1e-9:
            continue
        xa, xb = max(int(min(ax, bx, cx)), 0), min(int(max(ax, bx, cx)) + 2, w)
        ya, yb = max(int(min(ay, by, cy)), 0), min(int(max(ay, by, cy)) + 2, h)
        if xa >= xb or ya >= yb:
            continue
        X, Y = np.meshgrid(np.arange(xa, xb) + 0.5, np.arange(ya, yb) + 0.5)
        l1 = ((bx - X) * (cy - Y) - (by - Y) * (cx - X)) / area
        l2 = ((cx - X) * (ay - Y) - (cy - Y) * (ax - X)) / area
        l3 = 1 - l1 - l2
        z = l1 * dep[i, 0] + l2 * dep[i, 1] + l3 * dep[i, 2]
        up = (l1 >= 0) & (l2 >= 0) & (l3 >= 0) & (z > zbuf[ya:yb, xa:xb])
        zbuf[ya:yb, xa:xb][up] = z[up]
        tid[ya:yb, xa:xb][up] = i
        bary[ya:yb, xa:xb][up] = np.stack([l1[up], l2[up], l3[up]], 1)
    return tid, bary


def normals(tri):
    n = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
    return n / np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12)


def faces_of(tri, n):
    """A number per flat face: triangles in one plane share it, so the wire shows faces and not triangles."""
    keys = {}
    out = np.zeros(len(tri), int)
    for i in range(len(tri)):
        side = n[i] if n[i] @ np.array([0.31, 0.57, 0.76]) >= 0 else -n[i]
        out[i] = keys.setdefault(tuple(np.round(np.append(side, side @ tri[i, 0]), 2)), len(keys))
    return out


def occluded(o, d, tri):
    """Which rays from points o along d meet a triangle (Moeller-Trumbore), in chunks."""
    v0, e1, e2 = tri[:, 0], tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0]
    h = np.cross(d, e2)
    a = (e1 * h).sum(1)
    keep = np.abs(a) > 1e-9
    v0, e1, e2, h, a = v0[keep], e1[keep], e2[keep], h[keep], a[keep]
    hit = np.zeros(len(o), bool)
    for at in range(0, len(o), 4000):
        s = o[at:at + 4000, None] - v0[None]
        u = (s * h[None]).sum(2) / a
        q = np.cross(s, e1[None])
        v = (q @ d) / a
        t = (q * e2[None]).sum(2) / a
        hit[at:at + 4000] = ((u >= 0) & (v >= 0) & (u + v <= 1) & (t > 1e-3)).any(1)
    return hit


def down(a, ss):
    return a.reshape(a.shape[0] // ss, ss, a.shape[1] // ss, ss, *a.shape[2:]).mean((1, 3))


def rgba(rgb, alpha):
    return np.dstack([np.clip(rgb, 0, 255), alpha]).astype(np.uint8)


def view_layers(tri, uv, tex, tex_of, k, ss=3):
    """The computed layers of step k. tri: n x 3 x 3 world, uv: n x 3 x 2 (v from the bottom), tex: images top row first,
    tex_of: an image per triangle, or tex empty for a model without a texture."""
    box = canvas(tri, k)
    n = normals(tri)
    tid, bary = raster(tri, k, box, ss)
    on = tid >= 0
    ii = tid[on]
    # Lit by the world's sun, a face turned towards the eye as the model turns: an open board is lit on the side seen.
    eye = quarter(EYE.copy()[None], -k % 4)[0]
    facing = np.where((n @ eye)[:, None] < 0, -n, n)
    nn = facing[ii]
    pos = (bary[on][..., None] * tri[ii]).sum(1)
    cos = nn @ SUN
    gain = AMB + DIF * np.maximum(cos, 0)
    lit = cos > 0
    gain[lit] *= np.where(occluded(pos[lit] + nn[lit] * 0.4, SUN, tri), DENSITY, 1.0)
    light = np.ones(on.shape)
    light[on] = gain / PEAK
    H, W = box[3], box[2]
    cover = down(on.astype(float), ss)
    opaque = cover >= 0.5
    out = {'box': box, 'opaque': opaque}
    v = np.round(down(light, ss) * 255)
    out['light'] = rgba(np.dstack([v, v, v]), opaque * 255)
    if len(tex):
        rgb = np.zeros(on.shape + (3,))
        col = np.zeros((len(ii), 3))
        puv = (bary[on][..., None] * uv[ii]).sum(1)
        for t, im in enumerate(tex):
            sel = tex_of[ii] == t
            h, w = im.shape[:2]
            col[sel] = im[np.clip(((1 - puv[sel, 1]) * h).astype(int), 0, h - 1), np.clip((puv[sel, 0] * w).astype(int), 0, w - 1), :3]
        rgb[on] = col
        count = np.maximum(cover[..., None], 1e-6)
        out['texture'] = rgba(down(rgb, ss) / count * PEAK, opaque * 255)
    # The wire and the faces from the sample at each pixel's middle, so a line is one pixel wide on the true edge.
    mid = tid[ss // 2::ss, ss // 2::ss]
    face = np.where(mid >= 0, faces_of(tri, n)[np.maximum(mid, 0)], -1)
    edge = np.zeros(face.shape, bool)
    edge[:, :-1] |= face[:, :-1] != face[:, 1:]
    edge[:-1, :] |= face[:-1, :] != face[1:, :]
    black = np.zeros((H, W, 3))
    out['wire'] = rgba(black, edge * 255)
    hue = (face[..., None] * np.array([97, 57, 151]) + np.array([40, 90, 140])) % 216 + 40
    out['faces'] = rgba(hue, (face >= 0) * 255)
    shadow = raster(tri, k, box, 1, flat=True)[0] >= 0
    out['ground shadow'] = rgba(black, (shadow & ~opaque) * 128)
    zero = np.zeros((H, W, 4), np.uint8)
    zx, zy = -box[0], -box[1]
    for d in range(-6, 7):
        if 0 <= zy < H and 0 <= zx + d < W:
            zero[zy, zx + d] = (0, 255, 80, 255)
        if 0 <= zy + d < H and 0 <= zx < W:
            zero[zy + d, zx] = (0, 255, 80, 255)
    out['zero cross'] = zero
    return out


def png(a):
    h, w = a.shape[:2]
    raw = np.zeros((h, w * 4 + 1), np.uint8)
    raw[:, 1:] = a.reshape(h, w * 4)

    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw.tobytes(), 6)) + chunk(b'IEND', b'')


def over(bottom, layer, multiply):
    a = layer[..., 3:] / 255.0
    rgb = layer[..., :3].astype(float)
    if multiply:
        rgb = bottom[..., :3] * rgb / 255.0
    out = bottom.astype(float)
    out[..., :3] = rgb * a + out[..., :3] * (1 - a)
    out[..., 3:] = 255 * a + out[..., 3:] * (1 - a)
    return out


def write_ora(path, W, H, layers):
    """layers, top first: (name, image, visible, multiply). OpenRaster is what Krita and GIMP both open with its layers."""
    merged = np.zeros((H, W, 4))
    for name, im, visible, multiply in reversed(layers):
        if visible:
            merged = over(merged, im, multiply)
    merged = np.clip(merged, 0, 255).astype(np.uint8)
    stack = ''
    with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED) as z:
        z.writestr(zipfile.ZipInfo('mimetype'), 'image/openraster', zipfile.ZIP_STORED)
        for i, (name, im, visible, multiply) in enumerate(layers):
            z.writestr(f'data/{i:03}.png', png(im))
            stack += f'<layer name="{name}" src="data/{i:03}.png" x="0" y="0" opacity="1.0" visibility="{"visible" if visible else "hidden"}" composite-op="{"svg:multiply" if multiply else "svg:src-over"}"/>'
        z.writestr('stack.xml', f"<?xml version='1.0' encoding='UTF-8'?><image version='0.0.3' w='{W}' h='{H}'><stack>{stack}</stack></image>")
        z.writestr('mergedimage.png', png(merged))
        step = max(1, max(W, H) // 256 + 1)
        z.writestr('Thumbnails/thumbnail.png', png(merged[::step, ::step]))
    return merged


def pictures_for(variant_dir):
    """Per camera step, the 2003 pictures of the family that show that side, the variant's own first:
    {step: [(label, folder)]}. tools/zig/fixtures/bk-views/inventory.json knows which side each one shows."""
    d = os.path.abspath(variant_dir).replace(os.sep, '/')
    at = d.lower().rfind('/data/')
    root, rel = d[:at], d[at + 6:]
    out = {0: [(os.path.basename(d), d)]}
    try:
        inv = json.load(open(os.path.join(root, 'tools/zig/fixtures/bk-views/inventory.json')))
    except OSError:
        return out, root
    under = rel.split('/', 1)[1].lower() if rel.lower().startswith('buildings/') else None
    for fam in inv['families']:
        own = next((v for v in fam['variants'] if v['path'].lower() == under), None)
        if not own:
            continue
        for v in fam['variants']:
            k = (v['side'] - own['side']) % 4
            if v is own:
                continue
            out.setdefault(k, []).append((os.path.basename(v['path']), f"{root}/Data/{rel.split('/', 1)[0]}/{v['path']}"))
    return out, root


def laid(sprite, box, shift=(0, 0)):
    """A sprite on a view's canvas, its zero cross on the view's, moved by `shift`."""
    x0, y0, W, H = box
    out = np.zeros((H, W, 4), np.uint8)
    im = sprite['img']
    dx, dy = sprite['ox'] - x0 + shift[0], sprite['oy'] - y0 + shift[1]
    xa, ya = max(dx, 0), max(dy, 0)
    xb, yb = min(dx + im.shape[1], W), min(dy + im.shape[0], H)
    if xa < xb and ya < yb:
        out[ya:yb, xa:xb] = im[ya - dy:yb - dy, xa - dx:xb - dx]
    return out


def best_shift(sprite, box, opaque):
    """Where another side's picture lies on the model: its own zero cross is not this one's, so the outlines decide."""
    best, at = -1.0, (0, 0)
    for step, area in ((8, 200), (1, 8)):
        cx, cy = at
        for dy in range(-area, area + 1, step):
            for dx in range(-area, area + 1, step):
                a = laid(sprite, box, (cx + dx, cy + dy))[..., 3] >= 128
                iou = (a & opaque).sum() / max((a | opaque).sum(), 1)
                if iou > best:
                    best, at = iou, (cx + dx, cy + dy)
    return at, best


def write_templates(tri, uv, tex, tex_of, variant_dir, out_dir, log=print):
    """One <out>/step_<k>.ora a camera step, and layout.json with each canvas's zero cross."""
    os.makedirs(out_dir, exist_ok=True)
    pics, root = pictures_for(variant_dir) if variant_dir else ({}, None)
    if root:
        sys.path.insert(0, os.path.join(root, 'tools', 'house_model'))
        import bk1
    layout = {}
    for k in range(4):
        v = view_layers(tri, uv, tex, tex_of, k)
        x0, y0, W, H = v['box']
        blank = np.zeros((H, W, 4), np.uint8)
        layers = [('zero cross', v['zero cross'], False, False), ('wire', v['wire'], True, False), ('faces', v['faces'], False, False),
                  ('light', v['light'], False, True), ('paint here', blank, True, False)]
        for label, folder in pics.get(k, []):
            own = folder.endswith(os.path.basename(variant_dir.rstrip('/\\'))) and k == 0
            shift = (0, 0)
            for state in ('1', '2', '3', '1w', '2w', '3w'):
                if not os.path.exists(os.path.join(folder, state + '.san')):
                    continue
                sprite = bk1.pack(folder, state)[0]
                if state == '1' and not own:
                    shift, iou = best_shift(sprite, v['box'], v['opaque'])
                    log(f'step {k}: {label} laid by its outline, {shift}, overlap {iou:.2f}')
                layers.append((f'2003 {label} state {state}', laid(sprite, v['box'], shift), state == '1', False))
        if 'texture' in v:
            layers.append(('model texture', v['texture'], True, False))
        layers.append(('ground shadow', v['ground shadow'], True, False))
        write_ora(os.path.join(out_dir, f'step_{k}.ora'), W, H, layers)
        layout[f'step_{k}'] = dict(size=[W, H], zero=[-x0, -y0])
    json.dump(layout, open(os.path.join(out_dir, 'layout.json'), 'w'), indent=1)
    log(f'wrote {out_dir}: {len(tri)} triangles')
    return layout


def load_hand(path):
    """A hand model fixture as arrays, without Blender."""
    d = json.load(open(path))
    V = np.array(d['vertices'], float)
    T = np.array(d['triangles'], int)
    tri = V[T[:, :3], :3]
    uv = V[T[:, :3], 3:5] / [d['width'], d['height']]
    uv[..., 1] = 1 - uv[..., 1]
    return tri, uv, d


# -----------------------------------------------------------------------------------------------------------------------
# Blender: the panel, the BK1 camera, the fixture import, and reading a scene's meshes.
try:
    import bpy
    from mathutils import Matrix
except ImportError:
    bpy = None

if bpy:
    def scene_arrays(objects):
        tris, uvs, tex_of, tex = [], [], [], []
        seen = {}
        for ob in objects:
            me = ob.evaluated_get(bpy.context.evaluated_depsgraph_get()).to_mesh()
            me.calc_loop_triangles()
            m = np.array(ob.matrix_world)
            uvl = me.uv_layers.active
            for t in me.loop_triangles:
                co = np.array([me.vertices[i].co for i in t.vertices])
                tris.append(co @ m[:3, :3].T + m[:3, 3])
                uvs.append([uvl.data[i].uv for i in t.loops] if uvl else np.zeros((3, 2)))
                image = None
                if t.material_index < len(ob.material_slots) and ob.material_slots[t.material_index].material and ob.material_slots[t.material_index].material.node_tree:
                    image = next((n.image for n in ob.material_slots[t.material_index].material.node_tree.nodes if n.type == 'TEX_IMAGE' and n.image), None)
                if image is not None and image.name not in seen:
                    w, h = image.size
                    px = np.array(image.pixels[:], np.float32).reshape(h, w, -1)[::-1]
                    seen[image.name] = len(tex)
                    tex.append(px[..., :3] * 255)
                tex_of.append(seen[image.name] if image is not None else -1)
            ob.evaluated_get(bpy.context.evaluated_depsgraph_get()).to_mesh_clear()
        return np.array(tris, float), np.array(uvs, float), tex, np.array(tex_of, int)

    def import_hand(path):
        tri, uv, d = load_hand(path)
        name = os.path.splitext(os.path.basename(path))[0]
        me = bpy.data.meshes.new(name)
        me.from_pydata(tri.reshape(-1, 3).tolist(), [], [[i * 3, i * 3 + 1, i * 3 + 2] for i in range(len(tri))])
        layer = me.uv_layers.new(name='atlas')
        layer.data.foreach_set('uv', uv.reshape(-1))
        me.update()
        ob = bpy.data.objects.new(name, me)
        bpy.context.collection.objects.link(ob)
        atlas = os.path.join(os.path.dirname(path), name + '_1.png')
        if os.path.exists(atlas):
            mat = bpy.data.materials.new(name)
            mat.use_nodes = True
            n = mat.node_tree.nodes.new('ShaderNodeTexImage')
            n.image = bpy.data.images.load(atlas)
            mat.node_tree.links.new(n.outputs[0], mat.node_tree.nodes['Principled BSDF'].inputs[0])
            me.materials.append(mat)
        return ob

    def look_from(k):
        """The BK1 camera of step k, through the object's origin, at 1 Blender unit a world unit. Only for looking."""
        cam = bpy.data.objects.get('BK1 camera')
        if cam is None:
            cam = bpy.data.objects.new('BK1 camera', bpy.data.cameras.new('BK1 camera'))
            bpy.context.collection.objects.link(cam)
        cam.data.type = 'ORTHO'
        cam.data.ortho_scale = 400
        right, up, eye = R0, -R1, EYE
        turn = [quarter(a.copy()[None], -k % 4)[0] for a in (right, up, eye)]
        cam.matrix_world = Matrix([[turn[0][i], turn[1][i], turn[2][i], turn[2][i] * 800] for i in range(3)] + [[0, 0, 0, 1]])
        bpy.context.scene.camera = cam
        for area in bpy.context.screen.areas if bpy.context.screen else []:
            if area.type == 'VIEW_3D':
                area.spaces[0].region_3d.view_perspective = 'CAMERA'

    class BK1_OT_import_hand(bpy.types.Operator):
        """Load a tools/zig/fixtures/bk-views/hand-models/<name>.json"""
        bl_idname = 'bk1.import_hand'
        bl_label = 'Import hand model'
        filepath: bpy.props.StringProperty(subtype='FILE_PATH')
        filter_glob: bpy.props.StringProperty(default='*.json', options={'HIDDEN'})

        def invoke(self, context, event):
            context.window_manager.fileselect_add(self)
            return {'RUNNING_MODAL'}

        def execute(self, context):
            import_hand(self.filepath)
            return {'FINISHED'}

    class BK1_OT_look(bpy.types.Operator):
        """Look through the BK1 camera at this camera step"""
        bl_idname = 'bk1.look'
        bl_label = 'Look'
        step: bpy.props.IntProperty()

        def execute(self, context):
            look_from(self.step)
            return {'FINISHED'}

    class BK1_OT_templates(bpy.types.Operator):
        """Write a layered paint template per camera step from the selected meshes, or all if none is selected"""
        bl_idname = 'bk1.templates'
        bl_label = 'Write paint templates'

        def execute(self, context):
            s = context.scene
            obs = [o for o in (context.selected_objects or context.scene.objects) if o.type == 'MESH']
            if not obs or not s.bk1_out:
                self.report({'ERROR'}, 'Needs a mesh and an output folder')
                return {'CANCELLED'}
            tri, uv, tex, tex_of = scene_arrays(obs)
            lines = []
            write_templates(tri, uv, tex, tex_of, bpy.path.abspath(s.bk1_variant) or None, bpy.path.abspath(s.bk1_out), lines.append)
            self.report({'INFO'}, lines[-1])
            return {'FINISHED'}

    class BK1_PT_panel(bpy.types.Panel):
        bl_label = 'Blitzkrieg views'
        bl_space_type = 'VIEW_3D'
        bl_region_type = 'UI'
        bl_category = 'BK1'

        def draw(self, context):
            col = self.layout.column()
            col.operator('bk1.import_hand')
            row = col.row(align=True)
            for k in range(4):
                row.operator('bk1.look', text=f'Step {k}').step = k
            col.prop(context.scene, 'bk1_variant')
            col.prop(context.scene, 'bk1_out')
            col.operator('bk1.templates')

    CLASSES = (BK1_OT_import_hand, BK1_OT_look, BK1_OT_templates, BK1_PT_panel)

    def register():
        for c in CLASSES:
            bpy.utils.register_class(c)
        bpy.types.Scene.bk1_variant = bpy.props.StringProperty(name='2003 folder', subtype='DIR_PATH', description='The Data folder of this object, for its pictures (optional)')
        bpy.types.Scene.bk1_out = bpy.props.StringProperty(name='Output folder', subtype='DIR_PATH')

    def unregister():
        for c in CLASSES:
            bpy.utils.unregister_class(c)

    if __name__ == '__main__' and '--' in sys.argv:
        register()
        args = sys.argv[sys.argv.index('--') + 1:]
        opt = dict(zip(args[::2], args[1::2]))
        bpy.ops.wm.read_factory_settings(use_empty=True)
        ob = import_hand(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'zig', 'fixtures', 'bk-views', 'hand-models', opt['--hand'] + '.json'))
        write_templates(*scene_arrays([ob]), opt.get('--data'), opt['--out'])
