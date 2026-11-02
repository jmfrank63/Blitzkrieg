"""An orthographic CPU rasteriser at the 2003 camera: yaw 45, pitch -120, 1 pixel per world unit."""
import os, math, numpy as np
from PIL import Image

EXT = ''  # the folder a model's texture name is relative to; set by the caller
R0 = np.array([0.7071067812, 0.7071067812, 0.0])
R1 = np.array([0.3535533906, -0.3535533906, -0.8660254038])
EYE = np.array([0.6123724357, -0.6123724357, 0.5])
SUN = np.array([-1, -1, 2]) / math.sqrt(6)  # towards the sun, Data/consts.xml Direction (1,1,-2)
AMB, DIF = 0.6, 0.9
GRASS = np.array([104, 118, 70], float)
_tex = {}


def texture(rel):
    if rel in _tex:
        return _tex[rel]
    im = Image.open(os.path.join(EXT, rel))
    im.load()
    a = np.array(im.convert('RGBA'), dtype=np.uint8)
    _tex[rel] = a
    return a


def turn(p, k):
    p = p.copy()
    x, y = p[:, 0].copy(), p[:, 1].copy()
    for _ in range(k % 4):
        x, y = -y, x
    p[:, 0], p[:, 1] = x, y
    return p


def project(p):
    return np.stack([p @ R0, p @ R1], 1)


def sample(tex, uv, flipv=False):
    h, w = tex.shape[:2]
    u = uv[..., 0] % 1.0
    v = uv[..., 1] % 1.0
    if flipv:
        v = 1 - v
    x = u * w - 0.5
    y = v * h - 0.5
    x0 = np.floor(x).astype(int)
    y0 = np.floor(y).astype(int)
    fx = (x - x0)[..., None]
    fy = (y - y0)[..., None]
    c = lambda yy, xx: tex[yy % h, xx % w].astype(np.float64)
    return c(y0, x0) * (1 - fx) * (1 - fy) + c(y0, x0 + 1) * fx * (1 - fy) + c(y0 + 1, x0) * (1 - fx) * fy + c(y0 + 1, x0 + 1) * fx * fy


def nearest(tex, uv, flipv=False):
    h, w = tex.shape[:2]
    v = uv[..., 1] % 1.0
    if flipv:
        v = 1 - v
    x = (uv[..., 0] % 1.0 * w).astype(int) % w
    y = (v * h).astype(int) % h
    return tex[y, x]


def shadow_points(p):
    q = p.copy()
    t = q[:, 2] / SUN[2]
    q[:, 0] -= SUN[0] * t
    q[:, 1] -= SUN[1] * t
    q[:, 2] = 0
    return q


def extents(mod, k, S, with_shadow=True):
    P = turn(mod['P'] * S, k)
    s = project(P)
    lo, hi = s.min(0), s.max(0)
    if with_shadow:
        q = project(shadow_points(P))
        lo, hi = np.minimum(lo, q.min(0)), np.maximum(hi, q.max(0))
    return lo, hi


def _mats(mod):
    out = []
    for m in mod['mats']:
        if m and m.get('tex') and os.path.exists(os.path.join(EXT, m['tex'])):
            tex = texture(m['tex'])
        else:
            tex = np.full((4, 4, 4), 200, np.uint8)
        a = (m or {}).get('alpha', 'AM_OPAQUE')
        out.append(dict(tex=tex, alpha=a != 'AM_OPAQUE', two=(m or {}).get('twosided') == 'true'))
    if not out:
        out = [dict(tex=np.full((4, 4, 4), 200, np.uint8), alpha=False, two=True)]
    return out


def raster(mod, k, S, box, ss=4, flipv=False, shadow=False, use_light=True, cull=True, cull_sign=-1):
    """box: (x0, y0, w, h) in final pixels around the model origin."""
    x0, y0, W, H = box
    w, h = W * ss, H * ss
    P = turn(mod['P'] * S, k)
    Nn = turn(mod['N'], k)
    TM, UV, T = mod['TM'], mod['UV'], mod['T']
    mats = _mats(mod)
    Pg = shadow_points(P) if shadow else P
    s = project(Pg)
    sx = (s[:, 0] - x0) * ss
    sy = (s[:, 1] - y0) * ss
    dep = Pg @ EYE * ss
    zbuf = np.full((h, w), -1e18)
    tid = np.full((h, w), -1, int)
    B = np.zeros((h, w, 3))
    for i, tri in enumerate(T):
        a, b, c = tri
        ax, ay, bx, by, cx, cy = sx[a], sy[a], sx[b], sy[b], sx[c], sy[c]
        area = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax)
        if abs(area) < 1e-9:
            continue
        mt = mats[min(TM[i], len(mats) - 1)]
        if cull and not shadow and not mt['two'] and area * cull_sign < 0:
            continue
        xa = max(int(math.floor(min(ax, bx, cx))), 0)
        xb = min(int(math.ceil(max(ax, bx, cx))) + 1, w)
        ya = max(int(math.floor(min(ay, by, cy))), 0)
        yb = min(int(math.ceil(max(ay, by, cy))) + 1, h)
        if xa >= xb or ya >= yb:
            continue
        X, Y = np.meshgrid(np.arange(xa, xb) + 0.5, np.arange(ya, yb) + 0.5)
        l1 = ((bx - X) * (cy - Y) - (by - Y) * (cx - X)) / area
        l2 = ((cx - X) * (ay - Y) - (cy - Y) * (ax - X)) / area
        l3 = 1 - l1 - l2
        ins = (l1 >= 0) & (l2 >= 0) & (l3 >= 0)
        if not ins.any():
            continue
        z = l1 * dep[a] + l2 * dep[b] + l3 * dep[c]
        if mt['alpha']:
            uv = l1[..., None] * UV[a] + l2[..., None] * UV[b] + l3[..., None] * UV[c]
            ins &= nearest(mt['tex'], uv, flipv)[..., 3] >= 128
        sub = zbuf[ya:yb, xa:xb]
        up = ins & (z > sub)
        sub[up] = z[up]
        tid[ya:yb, xa:xb][up] = i
        B[ya:yb, xa:xb][up] = np.stack([l1[up], l2[up], l3[up]], 1)
    if shadow:
        return (tid >= 0).reshape(H, ss, W, ss).mean((1, 3))
    rgb = np.zeros((h, w, 3))
    m = tid >= 0
    ii = tid[m]
    bb = B[m]
    tv = T[ii]
    uv = bb[:, 0, None] * UV[tv[:, 0]] + bb[:, 1, None] * UV[tv[:, 1]] + bb[:, 2, None] * UV[tv[:, 2]]
    nr = bb[:, 0, None] * Nn[tv[:, 0]] + bb[:, 1, None] * Nn[tv[:, 1]] + bb[:, 2, None] * Nn[tv[:, 2]]
    nr /= np.maximum(np.linalg.norm(nr, axis=1, keepdims=True), 1e-9)
    col = np.zeros((len(ii), 3))
    for mi in np.unique(TM[ii]):
        sel = TM[ii] == mi
        mt = mats[min(mi, len(mats) - 1)]
        col[sel] = sample(mt['tex'], uv[sel], flipv)[:, :3]
    if use_light:
        col = col * (AMB + DIF * np.maximum(nr @ SUN, 0)[:, None])
    rgb[m] = np.clip(col, 0, 255)
    cov = m.reshape(H, ss, W, ss).mean((1, 3))
    tot = rgb.reshape(H, ss, W, ss, 3).sum((1, 3))
    n = m.reshape(H, ss, W, ss).sum((1, 3))[..., None]
    return tot / np.maximum(n, 1), cov, tid, B


def compose(mod, k, S, pad=10, ss=4, flipv=False, bg=GRASS, shadow_on=True, box=None, use_light=True, cull_sign=-1):
    if box is None:
        lo, hi = extents(mod, k, S)
        box = (int(lo[0]) - pad, int(lo[1]) - pad, int(hi[0] - lo[0]) + 2 * pad + 2, int(hi[1] - lo[1]) + 2 * pad + 2)
    rgb, cov, _, _ = raster(mod, k, S, box, ss, flipv, use_light=use_light, cull_sign=cull_sign)
    x0, y0, W, H = box
    img = np.tile(bg, (H, W, 1)).astype(float)
    if shadow_on:
        sh = raster(mod, k, S, box, ss, flipv, shadow=True)
        img *= (1 - 0.5 * (sh >= 0.5))[..., None]
    opaque = cov >= 0.5
    img[opaque] = rgb[opaque]
    return img.astype(np.uint8), box, opaque
