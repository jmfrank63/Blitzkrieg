"""What every hand model shares: flat faces with their own texture rectangle, the one atlas, the fill of small gaps, and the
outputs. See docs/superpowers/specs/2026-11-02-hand-model-from-picture.md."""
import json, math, os, pickle
import numpy as np
from PIL import Image, ImageFilter
import rast, bk1, castshadow, handexport

T = 2.0  # texels per world unit
# The sun's full contrast burns painted art (a roof turned to the sun came out 1.6x its 2003 brightness); this keeps the
# lit and shaded sides apart at about 1.2x.
rast.AMB, rast.DIF = 0.8, 0.35
OUT = None


def output(folder):
    """Everything goes to zig-out/local-test/<folder>."""
    global OUT
    OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'zig-out', 'local-test', folder).replace(os.sep, '/')
    os.makedirs(OUT, exist_ok=True)
    rast.EXT = OUT


def shade(n):
    return rast.AMB + rast.DIF * max(float(np.dot(n, rast.SUN)), 0.0)


def wave(v, a, b):
    """Mirrored tiling of v into [a, b]: art repeats without a seam."""
    w = b - a
    t = (v - a) % (2 * w)
    return a + (t if t <= w else 2 * w - t)


faces = []


class Face:
    def __init__(self, name, normal, polys, src=None):
        self.name, self.polys, self.src = name, [np.array(p, float) for p in polys], src
        n = np.array(normal, float)
        self.n = n / np.linalg.norm(n)
        # local frame: V as near "down the screen" as the plane allows, so a texture reads upright
        up = np.array([0, 0, 1.0]) if abs(self.n[2]) < .95 else np.array([1.0, 0, 0])
        self.v = up - self.n * np.dot(up, self.n)
        self.v /= np.linalg.norm(self.v)
        self.u = np.cross(self.v, self.n)
        pts = np.concatenate(self.polys)
        self.o = pts[0]
        uu, vv = (pts - self.o) @ self.u, (pts - self.o) @ self.v
        self.umin, self.vmin = uu.min(), vv.min()
        self.w = max(int(math.ceil((uu.max() - uu.min()) * T)) + 1, 2)
        self.h = max(int(math.ceil((vv.max() - vv.min()) * T)) + 1, 2)
        self.tex = np.zeros((self.h, self.w, 3))
        self.seen = np.zeros((self.h, self.w), bool)
        faces.append(self)

    def world(self):
        X, Y = np.meshgrid((np.arange(self.w) + .5) / T + self.umin, (np.arange(self.h) + .5) / T + self.vmin)
        return self.o + X[..., None] * self.u + Y[..., None] * self.v

    def lookup(self, q):
        """The albedo at world point q, read from this face."""
        d = q - self.o
        ix = np.clip(((d @ self.u - self.umin) * T).astype(int), 0, self.w - 1)
        iy = np.clip(((d @ self.v - self.vmin) * T).astype(int), 0, self.h - 1)
        return self.tex[iy, ix]


def rect_x(x, y0, y1, z0, z1):
    return [(x, y0, z0), (x, y1, z0), (x, y1, z1), (x, y0, z1)]


def rect_y(y, x0, x1, z0, z1):
    return [(x0, y, z0), (x1, y, z0), (x1, y, z1), (x0, y, z1)]


def normal_of(poly, hint):
    p0, p1, p2 = (np.array(v, float) for v in poly[:3])
    n = np.cross(p1 - p0, p2 - p0)
    return n if n @ np.array(hint, float) > 0 else -n


def wv(v, a, b):
    return np.vectorize(lambda s: wave(s, a, b))(v)


# Assemble the model with a provisional atlas, to learn which face each picture pixel belongs to.
def build(pic=False):
    """One atlas with gutters; flat normals, so every face keeps its own vertices."""
    G = 3
    order = sorted(faces, key=lambda f: -f.h)
    W = 1024
    x = y = rowh = 0
    for f in order:
        if x + f.w + 2 * G > W:
            x, y, rowh = 0, y + rowh, 0
        f.ax, f.ay = x + G, y + G
        x += f.w + 2 * G
        rowh = max(rowh, f.h + 2 * G)
    H = y + rowh
    atlas = np.zeros((H, W, 3), np.uint8)
    P, N, UV, TT, FI = [], [], [], [], []
    for i, f in enumerate(faces):
        t = np.clip(f.tex, 0, 255).astype(np.uint8)
        pad = np.pad(t, ((G, G), (G, G), (0, 0)), mode='edge')
        atlas[f.ay - G:f.ay + f.h + G, f.ax - G:f.ax + f.w + G] = pad
        for poly in (getattr(f, 'polys_pic', f.polys) if pic else f.polys):
            base = len(P)
            for q in poly:
                d = q - f.o
                P.append(q)
                N.append(f.n)
                UV.append(((f.ax + (d @ f.u - f.umin) * T) / W, (f.ay + (d @ f.v - f.vmin) * T) / H))
            for k_ in range(1, len(poly) - 1):
                tri = (base, base + k_, base + k_ + 1)
                a, b, c = np.array(P[tri[0]]), np.array(P[tri[1]]), np.array(P[tri[2]])
                if np.cross(b - a, c - a) @ f.n < 0:
                    tri = (tri[0], tri[2], tri[1])
                TT.append(tri)
                FI.append(i)
    Image.fromarray(atlas).save(OUT + '/atlas.png')
    rast._tex.pop('atlas.png', None)
    return dict(P=np.array(P), N=np.array(N), UV=np.array(UV), T=np.array(TT), TM=np.zeros(len(TT), int),
                mats=[dict(tex='atlas.png', alpha='AM_OPAQUE', twosided='false')]), np.array(FI), atlas


def fill_nearest(f):
    """Small unseen gaps of a seen face (behind a post, at an edge) take the nearest seen texel of that same face."""
    if not f.seen.any():
        return False
    seen = f.seen.copy()
    t = f.tex
    for _ in range(max(f.w, f.h)):
        if seen.all():
            break
        g = np.zeros(seen.shape + (3,))
        c = np.zeros(seen.shape)
        for dy, dx in ((0, 1), (0, -1), (1, 0), (-1, 0)):
            ss = np.roll(seen, (dy, dx), (0, 1))
            tt = np.roll(t, (dy, dx), (0, 1))
            if dy == 1:
                ss[0] = False
            if dy == -1:
                ss[-1] = False
            if dx == 1:
                ss[:, 0] = False
            if dx == -1:
                ss[:, -1] = False
            g += tt * ss[..., None]
            c += ss
        new = ~seen & (c > 0)
        t[new] = g[new] / c[new][..., None]
        seen |= new
    return True


def finish(name, BK, mod, FI, atlas, PIC, OX, OY, opaque, iou, report):
    """The renders, the measures, the .obj, and the model bk-views reads."""
    H, W = PIC.shape[:2]
    tris = int(len(mod['T']))
    print('triangles', tris, 'vertices', len(mod['P']), 'atlas', atlas.shape[:2], 'faces', len(faces))
    # Four turns at the BK1 camera, 1 pixel per world unit.
    os.makedirs(OUT + '/turn', exist_ok=True)
    res = dict(tris=tris, verts=len(mod['P']), faces=len(faces), iou=iou, atlas=list(atlas.shape[:2]), seen=report)
    for t in range(4):
        im, box, op = castshadow.compose(mod, t, 1.0)
        Image.fromarray(im).resize((im.shape[1] * 2, im.shape[0] * 2), Image.NEAREST).save(f'{OUT}/turn/t{t}.png')
        res.setdefault('dims', []).append([im.shape[1], im.shape[0]])
    # Turn 0 against the picture, pixel by pixel, on the pixels both draw.
    rgb, cov, tid, _ = rast.raster(mod, 0, 1.0, (OX, OY, W, H), 4)
    both = opaque & (cov >= .5)
    err = np.abs(rgb[both] - PIC[..., :3][both].astype(float)).mean()
    res['turn0_mean_abs_error'] = float(err)
    res['turn0_iou'] = float((opaque & (cov >= .5)).sum() / (opaque | (cov >= .5)).sum())
    # The 2003 shadow against the model's.
    shp = bk1.pack(BK, '1s')[0]
    sh = rast.raster(mod, 0, 1.0, (shp['ox'], shp['oy'], shp['img'].shape[1], shp['img'].shape[0]), 2, shadow=True) >= .5
    sa = shp['img'][..., 3] > 0
    from PIL import ImageFilter as IF
    # the 2003 shadow is a dither of opaque pixels; close it before comparing areas
    closed = Image.fromarray((sa * 255).astype(np.uint8)).filter(IF.MaxFilter(3)).filter(IF.MinFilter(3))
    sa = np.array(closed) > 128
    res['shadow_iou'] = float((sa & sh).sum() / (sa | sh).sum())
    # Footprint against passability: locked cells of the 2003 data against the walls' ground plan.
    print({k: v for k, v in res.items() if k != 'seen'})
    print(report)
    json.dump(res, open(OUT + '/result.json', 'w'), indent=0)
    # Wireframe data and an .obj for Blender.
    with open(OUT + '/' + name + '.obj', 'w') as o:
        o.write(f'# {name}, written by hand from the 2003 picture\nmtllib {name}.mtl\nusemtl house\n')
        for p in mod['P']:
            o.write('v %.3f %.3f %.3f\n' % tuple(p))
        for uv in mod['UV']:
            o.write('vt %.5f %.5f\n' % (uv[0], 1 - uv[1]))
        for n in mod['N']:
            o.write('vn %.4f %.4f %.4f\n' % tuple(n))
        for t_ in mod['T'] + 1:
            o.write('f %d/%d/%d %d/%d/%d %d/%d/%d\n' % (t_[0], t_[0], t_[0], t_[1], t_[1], t_[1], t_[2], t_[2], t_[2]))
    json.dump(dict(names=[f.name for f in faces], FI=FI.tolist()), open(OUT + '/faces.json', 'w'))
    import pickle
    pickle.dump(mod, open(OUT + '/mod.pkl', 'wb'))
    handexport.write(name, faces, mod, FI, atlas)
