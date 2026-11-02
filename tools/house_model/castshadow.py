"""Cast shadows computed from the model and the world sun, at every turn: the overhang on the walls, the dormers and the chimney
on the roof. The sun stays in the world while the house turns, so nothing here can be painted into a texture."""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # python -I leaves the script's folder off the path
import numpy as np
import rast

DENSITY = 0.6  # a lit surface in shadow keeps this share of its light; the ground shadow's 0.5 is for flat grass


def occluded(o, d, tri):
    """Moller-Trumbore: which rays from points o along d hit any triangle. Chunked, a house has about a hundred triangles."""
    v0, e1, e2 = tri[:, 0], tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0]
    hit = np.zeros(len(o), bool)
    h = np.cross(d, e2)
    a = (e1 * h).sum(1)
    keep = np.abs(a) > 1e-9
    v0, e1, e2, h, a = v0[keep], e1[keep], e2[keep], h[keep], a[keep]
    for i0 in range(0, len(o), 20000):
        oo = o[i0:i0 + 20000]
        s = oo[:, None, :] - v0[None, :, :]
        u = (s * h[None]).sum(2) / a
        q = np.cross(s, e1[None])
        v = (q @ d) / a
        t = (q * e2[None]).sum(2) / a
        hit[i0:i0 + 20000] = ((u >= 0) & (v >= 0) & (u + v <= 1) & (t > 1e-3)).any(1)
    return hit


def factor(mod, k, S, box, ss=3):
    """The light kept per final pixel, averaged over its samples, and the coverage."""
    _, cov, tid, B = rast.raster(mod, k, S, box, ss)
    P = rast.turn(mod['P'] * S, k)
    T = mod['T']
    tri = P[T]
    fn = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
    fn /= np.maximum(np.linalg.norm(fn, axis=1, keepdims=True), 1e-9)
    m = tid >= 0
    ii = tid[m]
    pos = (B[m][..., None] * tri[ii]).sum(1)
    lit = fn[ii] @ rast.SUN > 0
    keep = np.ones(len(ii))
    if lit.any():
        o = pos[lit] + fn[ii][lit] * (.4 * S)
        keep_l = np.where(occluded(o, rast.SUN, tri), DENSITY, 1.0)
        keep[lit] = keep_l
    f = np.ones(tid.shape)
    f[m] = keep
    x0, y0, W, H = box
    return f.reshape(H, ss, W, ss).mean((1, 3))


def compose(mod, k, S, pad=10, ss=4, bg=rast.GRASS, shadow_on=True, box=None, **_):
    if box is None:
        lo, hi = rast.extents(mod, k, S)
        box = (int(lo[0]) - pad, int(lo[1]) - pad, int(hi[0] - lo[0]) + 2 * pad + 2, int(hi[1] - lo[1]) + 2 * pad + 2)
    rgb, cov, _, _ = rast.raster(mod, k, S, box, ss)
    x0, y0, W, H = box
    img = np.tile(bg, (H, W, 1)).astype(float)
    if shadow_on:
        sh = rast.raster(mod, k, S, box, ss, shadow=True)
        img *= (1 - 0.5 * (sh >= 0.5))[..., None]
    opaque = cov >= 0.5
    rgb = rgb * factor(mod, k, S, box)[..., None]
    img[opaque] = rgb[opaque]
    return img.astype(np.uint8), box, opaque
