"""e_house01_3 written by hand from the 2003 picture, as its relative e_house01_1: the same body, roof and chimney, its own
gabled dormers and shingles. Every visible face takes the 2003 pixels it shows. A hidden face reuses art of a named seen face at the same scale,
so no texel is stretched or invented. Coordinates: world units, z up, the zero cross at the porch post's foot."""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # python -I leaves the script's folder off the path
from hand import *
from house01 import *
output('house_model_3')
BK = bk1.ROOT + '/Buildings/europe/summer/e_house01_3'
sp = bk1.pack(BK, '1')[0]
# Its outline lies a pixel right of and above e_house01_1's on the same canvas: the body is the same, read one pixel over.
PIC, OX, OY = sp['img'], sp['ox'] - 1, sp['oy'] + 1


# The two gabled dormers, solved on the picture. The window wall stands at FX. Each roof slope runs from the ridge DM (height
# ZDR) down to its eave (DA, height ZDE); its back corners lie in the main slope, so the valleys run from DBX to DRX.
FX, DWY = -13.1, 16.0
DORMERS = [(34.3, 1), (80.0, 1)]
DBX, DRX = -33.9, -49.85
ZDE, ZDR = zf(DBX), zf(DRX)
ZF = zf(FX)
WIN, WIN_Z = ((-69.6, -60.2), (-53.6, -45.0)), (62.8, 78.0)  # the two gable windows


# Walls.
near = Face('gable near', (0, -1, 0), [profile(0.0, WX, PX)])
right = Face('wall right', (1, 0, 0), [rect_x(0.0, PY, WY, 0, zf(0))])
porch_x = Face('porch wall x', (1, 0, 0), [rect_x(PX, 0.0, PY, 0, zf(PX))])
porch_y = Face('porch wall y', (0, -1, 0), [profile(PY, PX, 0.0)])
floor = Face('porch floor', (0, 0, 1), [[(PX, 0, .5), (0, 0, .5), (0, PY, .5), (PX, PY, .5)]])
# The roof: the front slope has real holes where the dormers stand.
def front_quads(y0, y1):
    out = []
    prev = y0
    for m, _ in DORMERS:
        a_, b_ = m - DWY, m + DWY
        out += [slope_quad(XR, XE, prev, a_), slope_quad(FX, XE, a_, b_)]
        out += [[(XR, a_, zf(XR)), (DBX, a_, zf(DBX)), (DRX, m, zf(DRX)), (XR, m, zf(XR))],
                [(XR, m, zf(XR)), (DRX, m, zf(DRX)), (DBX, b_, zf(DBX)), (XR, b_, zf(XR))]]
        prev = b_
    return out + [slope_quad(XR, XE, prev, y1)]


quads = front_quads(ROOF_Y0, ROOF_Y1)
kk = (ZR - ZE) / (XE - XR)
front = Face('roof front', (kk, 0, 1), quads)
verge_near = Face('verge near', (0, -1, 0), [[(XE, ROOF_Y0, ZE - SLAB), (XR, ROOF_Y0, ZR - SLAB), (XR, ROOF_Y0, ZR), (XE, ROOF_Y0, ZE)],
                                                [(XR, ROOF_Y0, ZR - SLAB), (XB, ROOF_Y0, ZBE - SLAB), (XB, ROOF_Y0, ZBE), (XR, ROOF_Y0, ZR)]])
eave_front = Face('eave front', (1, 0, 0), [rect_x(XE, ROOF_Y0, ROOF_Y1, ZE - SLAB, ZE)])
dfaces = []
for i, (m, _) in enumerate(DORMERS):
    a, b = m - DWY, m + DWY
    w = Face(f'dormer {i} front', (1, 0, 0), [[(FX, a, ZF), (FX, b, ZF), (FX, b, ZDE), (FX, m, ZDR), (FX, a, ZDE)]])
    s_ = [(DRX, m, ZDR), (FX, m, ZDR), (FX, a, ZDE), (DBX, a, ZDE)]
    r = Face(f'dormer {i} roof', normal_of(s_, (0, -1, 1)), [s_])
    c = Face(f'dormer {i} cheek', (0, -1, 0), [[(DBX, a, ZDE), (FX, a, ZDE), (FX, a, ZF)]])
    dfaces.append((w, r, c, a, b, m))
def pic_polys(f, polys):
    f.polys_pic = [np.array(p_, float) for p_ in polys]


pic_polys(front, front_quads(PIC_Y0, PIC_Y1))
pic_polys(right, [rect_x(0.0, PY, WY_PIC, 0, zf(0))])
pic_polys(eave_front, [rect_x(XE, PIC_Y0, PIC_Y1, ZE - SLAB, ZE)])
verge_near.pic_shift = np.array([0.0, PIC_Y0 - ROOF_Y0, 0.0])
pic_polys(verge_near, [p_ + verge_near.pic_shift for p_ in verge_near.polys])
post_x = Face('post x', (1, 0, 0), [rect_x(0.0, 0.0, 3.0, 0, ZE - SLAB + 3)])
post_y = Face('post y', (0, -1, 0), [rect_y(0.0, -3.0, 0.0, 0, ZE - SLAB + 3)])
# The chimney stands behind the ridge on the back slope, which is why the picture shows only its tip: its corners at
# these numbers land on the tip's corners within a pixel. The shape below the tip follows e_house01_2, which shows a whole
# one: a stack, a wider collar, two flue pots.
CX0, CX1, CY0, CY1, CZ0 = -77.7, -67.7, 46.5, 56.5, 86.0
ZC0, ZC1, CO, ZP = 107.0, 109.8, 0.8, 115.6
chim_x = Face('chimney x', (1, 0, 0), [rect_x(CX1, CY0, CY1, CZ0, ZC0)])
chim_y = Face('chimney y', (0, -1, 0), [rect_y(CY0, CX0, CX1, CZ0, ZC0)])
coll_x = Face('collar x', (1, 0, 0), [rect_x(CX1 + CO, CY0 - CO, CY1 + CO, ZC0, ZC1)])
coll_y = Face('collar y', (0, -1, 0), [rect_y(CY0 - CO, CX0 - CO, CX1 + CO, ZC0, ZC1)])
coll_t = Face('collar top', (0, 0, 1), [[(CX0 - CO, CY0 - CO, ZC1), (CX1 + CO, CY0 - CO, ZC1), (CX1 + CO, CY1 + CO, ZC1), (CX0 - CO, CY1 + CO, ZC1)]])
POTS = [(-72.7 - 1.3, -72.7 + 1.3, y - 1.3, y + 1.3) for y in (49.0, 54.0)]
pot_faces = []
for i, (a, b, c, d) in enumerate(POTS):
    pot_faces.append((Face(f'pot {i} x', (1, 0, 0), [rect_x(b, c, d, ZC1, ZP)]), Face(f'pot {i} y', (0, -1, 0), [rect_y(c, a, b, ZC1, ZP)]),
                      Face(f'pot {i} top', (0, 0, 1), [[(a, c, ZP), (b, c, ZP), (b, d, ZP), (a, d, ZP)]])))
SEEN_FACES = list(faces)


# Hidden faces: each names the seen face and the mapping it reuses.
# The slope's clean band beside the near dormer, its one 2003 blot (y 5.5 to 8.5) left out. Courses run along y, so art mirrored
# along them keeps each course at its own row, with no seam across the slope.
def on_clean(a, b):
    v = np.vectorize(lambda s_: wave(s_, 0.0, 14.5))(b)
    y = np.where(v < 5.0, .5 + v, 8.5 + v - 5.0)
    return np.stack([a, y, zf(a)], -1)


def from_back(f, q):
    # distance down the back slope along the surface, at the same texel scale, mirrored in the long even strip above the dormers
    dist = (ZR - q[..., 2]) / (ZR - ZBE) * math.hypot(XR - XB, ZR - ZBE)
    x = XR + dist * (XE - XR) / math.hypot(XE - XR, ZR - ZE)
    # each fold of the courses starts elsewhere along them, or the two mirrors meet in a rosette
    y = q[..., 1] + 17.0 * np.floor((x + 64.0) / 11.0)
    x = wv(x, -64.0, -53.0)
    return np.stack([x, wv(y, 48.0, 92.0), zf(x)], -1)


def far_gable(f, q):
    x = q[..., 0]
    x2 = np.where(x > PX, wv(2 * PX - x, WX, PX), x)
    return np.stack([x2, np.zeros_like(x), np.minimum(q[..., 2], zroof_v(x2) - .1)], -1)


zroof_v = np.vectorize(zroof)


back = Face('roof back', (-(ZR - ZBE) / (XR - XB), 0, 1), [[(XR, ROOF_Y1, ZR), (XB, ROOF_Y1, ZBE), (XB, ROOF_Y0, ZBE), (XR, ROOF_Y0, ZR)]], ('front', from_back))
far = Face('gable far', (0, 1, 0), [profile(WY, WX, 0.0)], ('near', far_gable))
wall_b = Face('wall back', (-1, 0, 0), [rect_x(WX, 0.0, WY, 0, zb(WX))], None)
verge_far = Face('verge far', (0, 1, 0), [[(XE, ROOF_Y1, ZE - SLAB), (XR, ROOF_Y1, ZR - SLAB), (XR, ROOF_Y1, ZR), (XE, ROOF_Y1, ZE)],
                                               [(XR, ROOF_Y1, ZR - SLAB), (XB, ROOF_Y1, ZBE - SLAB), (XB, ROOF_Y1, ZBE), (XR, ROOF_Y1, ZR)]], ('verge_near', lambda f, q: np.stack([q[..., 0], np.full(q.shape[:2], ROOF_Y0), q[..., 2]], -1)))
eave_back = Face('eave back', (-1, 0, 0), [rect_x(XB, ROOF_Y0, ROOF_Y1, ZBE - SLAB, ZBE)], ('eave_front', lambda f, q: np.stack([np.full(q.shape[:2], XE), q[..., 1], ZE - (ZBE - q[..., 2])], -1)))
under_b = Face('underside back', ((ZR - ZBE) / (XR - XB), 0, -1), [[(XR, ROOF_Y0, ZR - SLAB), (XB, ROOF_Y0, ZBE - SLAB), (XB, ROOF_Y1, ZBE - SLAB), (XR, ROOF_Y1, ZR - SLAB)]])
under_f = Face('underside front', (-kk, 0, -1), [[(XE, ROOF_Y0, ZE - SLAB), (XR, ROOF_Y0, ZR - SLAB), (XR, ROOF_Y1, ZR - SLAB), (XE, ROOF_Y1, ZE - SLAB)]])
HID = []
for i, (w, r, c, a, b, m) in enumerate(dfaces):
    # The far slope and cheek are the near ones mirrored about the ridge.
    far_s = [(DRX, m, ZDR), (FX, m, ZDR), (FX, b, ZDE), (DBX, b, ZDE)]
    mirror = lambda f_, q, k=a + b: np.stack([q[..., 0], k - q[..., 1], q[..., 2]], -1)
    HID.append(Face(f'dormer {i} roof far', normal_of(far_s, (0, 1, 1)), [far_s], (r, mirror)))
    HID.append(Face(f'dormer {i} cheek far', (0, 1, 0), [[(DBX, b, ZDE), (FX, b, ZDE), (FX, b, ZF)]], (c, mirror)))
post_mx = Face('post -x', (-1, 0, 0), [rect_x(-3.0, 0.0, 3.0, 0, ZE - SLAB + 3)], (post_x, lambda f, q: np.stack([np.zeros(q.shape[:2]), q[..., 1], q[..., 2]], -1)))
post_py = Face('post +y', (0, 1, 0), [rect_y(3.0, -3.0, 0.0, 0, ZE - SLAB + 3)], (post_y, lambda f, q: np.stack([q[..., 0], np.zeros(q.shape[:2]), q[..., 2]], -1)))
chim_mx = Face('chimney -x', (-1, 0, 0), [rect_x(CX0, CY0, CY1, CZ0, ZC0)], (chim_x, lambda f, q: np.stack([np.full(q.shape[:2], CX1), q[..., 1], q[..., 2]], -1)))
chim_py = Face('chimney +y', (0, 1, 0), [rect_y(CY1, CX0, CX1, CZ0, ZC0)], (chim_y, lambda f, q: np.stack([q[..., 0], np.full(q.shape[:2], CY0), q[..., 2]], -1)))
coll_mx = Face('collar -x', (-1, 0, 0), [rect_x(CX0 - CO, CY0 - CO, CY1 + CO, ZC0, ZC1)], (coll_x, lambda f, q: np.stack([np.full(q.shape[:2], CX1 + CO), q[..., 1], q[..., 2]], -1)))
coll_py = Face('collar +y', (0, 1, 0), [rect_y(CY1 + CO, CX0 - CO, CX1 + CO, ZC0, ZC1)], (coll_y, lambda f, q: np.stack([q[..., 0], np.full(q.shape[:2], CY0 - CO), q[..., 2]], -1)))
for i, (a, b, c, d) in enumerate(POTS):
    px_, py_, _t = pot_faces[i]
    Face(f'pot {i} -x', (-1, 0, 0), [rect_x(a, c, d, ZC1, ZP)], (px_, lambda f, q, b=b: np.stack([np.full(q.shape[:2], b), q[..., 1], q[..., 2]], -1)))
    Face(f'pot {i} +y', (0, 1, 0), [rect_y(d, a, b, ZC1, ZP)], (py_, lambda f, q, c=c: np.stack([q[..., 0], np.full(q.shape[:2], c), q[..., 2]], -1)))
NAMED = dict(front=front, near=near, verge_near=verge_near, eave_front=eave_front)


for f in faces:
    f.tex[:] = 128
mod, FI, _ = build(pic=True)
H, W = PIC.shape[:2]
_, cov, tid, _ = rast.raster(mod, 0, 1.0, (OX, OY, W, H), 1)
owner = np.where(tid >= 0, FI[np.maximum(tid, 0)], -1)
opaque = PIC[..., 3] >= 128
model_mask = tid >= 0
inter = (opaque & model_mask).sum()
iou = float(inter / (opaque | model_mask).sum())
print('silhouette IoU at turn 0', round(iou, 3))

# Visible faces take the picture's pixels, divided by the light the picture was lit with.
for i, f in enumerate(faces):
    if f not in SEEN_FACES:
        continue
    q = f.world() + getattr(f, 'pic_shift', 0.0)
    s = rast.project(q.reshape(-1, 3)).reshape(q.shape[0], q.shape[1], 2)
    ix = np.floor(s[..., 0] - OX).astype(int)
    iy = np.floor(s[..., 1] - OY).astype(int)
    ok = (ix >= 0) & (ix < W) & (iy >= 0) & (iy < H)
    own = np.zeros(ok.shape, bool)
    own[ok] = (owner[iy[ok], ix[ok]] == i) & opaque[iy[ok], ix[ok]]
    f.tex[own] = PIC[iy[own], ix[own], :3] / shade(f.n)
    f.seen = own
    if f is right:
        f.seen &= q[..., 1] <= WY_PIC - .5
    if f is front:
        f.seen &= q[..., 1] >= PIC_Y0


# Below the tip the chimney takes e_house01_2's masonry, its cream and olive faces, toned to this house's own tip.
sib = bk1.pack(bk1.ROOT + '/Buildings/europe/summer/e_house01_2', '1')[0]
SA, SOX, SOY = sib['img'], sib['ox'], sib['oy']


def sibling(side, s_, h):
    """side 0: the lit -y face, 1: the +x face. s_ 0 at the front corner to 1 at the outer one, h world units below the top."""
    w = -6.0 if side == 0 else 9.5
    sx = 12.0 + w * s_
    sy = -122.0 - .5 * np.abs(w) * s_ + np.vectorize(lambda v: wave(v, 0.0, 20.0))(h) * .866
    return SA[np.clip((sy - SOY).astype(int), 0, SA.shape[0] - 1), np.clip((sx - SOX).astype(int), 0, SA.shape[1] - 1), :3].astype(float)


own_tip = {}
for f, side, axis in ((chim_y, 0, 0), (chim_x, 1, 1), (coll_y, 0, 0), (coll_x, 1, 1)):
    q = f.world()
    s_ = (CX1 - q[..., 0]) / (CX1 - CX0) if axis == 0 else (q[..., 1] - CY0) / (CY1 - CY0)
    v = sibling(side, np.clip(s_, 0, 1), np.maximum(ZC1 - q[..., 2], 0)) / shade(f.n)
    m = f.seen
    gain = 1.0
    if m.sum() >= 8:
        gain = float(np.clip((f.tex[m] @ [.3, .6, .1]).mean() / max((v[m] @ [.3, .6, .1]).mean(), 1), .75, 1.0))
    own_tip[f.name] = [int(m.sum()), round(gain, 2)]
    f.tex[~m] = np.clip(v * gain, 0, 255)[~m]
    f.seen[:] = True
print('chimney: own tip texels and tone gain', own_tip)
report = {}
for f in SEEN_FACES:
    report[f.name] = round(float(f.seen.mean()), 3)
# The slope's parts the dormers hide take the slope's own clean art before anything else fills.
q = front.world()
hidden = ~front.seen
if hidden.any():
    good = front.lookup(on_clean(q[..., 0], q[..., 1]))
    # The dormers' bright ridges and painted shadows lie just past their edges in the picture: all of that goes too.
    from PIL import ImageFilter
    dorm = np.array([i_ for i_, f_ in enumerate(faces) if f_.name.startswith('dormer')])
    near_dorm = np.array(Image.fromarray((np.isin(owner, dorm) * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(7))) > 128
    s_ = rast.project(q.reshape(-1, 3)).reshape(q.shape[0], q.shape[1], 2)
    ix_, iy_ = np.floor(s_[..., 0] - OX).astype(int), np.floor(s_[..., 1] - OY).astype(int)
    ok_ = (ix_ >= 0) & (ix_ < W) & (iy_ >= 0) & (iy_ < H)
    big = hidden.copy()
    big[ok_] |= near_dorm[iy_[ok_], ix_[ok_]]
    front.tex[big] = good[big]
    front.seen = front.seen | big
for f in SEEN_FACES:
    if not fill_nearest(f):
        # a face the picture never shows (the top of a thin chimney): the nearest art of the same material
        f.tex[:] = np.array([60, 45, 35]) / shade(f.n)
    f.seen[:] = True

# The picture lights each dormer's near slope far beyond what its sun gives; the light is computed, so half of the way to the main slope's tone keeps it reading as lit at turn 0.
main_tone = (front.tex.reshape(-1, 3) @ [.3, .6, .1]).mean()
for w, r, c, a, b, m in dfaces:
    r.tex[:] = r.tex * (main_tone / max((r.tex.reshape(-1, 3) @ [.3, .6, .1]).mean(), 1)) ** .5

# The picture paints the gable's top in the roof's shadow and hides the rest behind the roof. Above the top timber only the
# two windows and the middle post are kept; the rest continues the bay below it, the same posts and plaster in even light.
q = near.world()
x_, z_ = q[..., 0], q[..., 2]
plaster = near.lookup(np.stack([x_, q[..., 1], wv(z_, 37.0, 49.0)], -1))
keep = ((x_ > WIN[0][0]) & (x_ < WIN[0][1]) | (x_ > WIN[1][0]) & (x_ < WIN[1][1])) & (z_ > WIN_Z[0]) & (z_ < WIN_Z[1])
post = (x_ > -60.0) & (x_ < -54.2)
top = z_ > 59.0
near.tex[top & ~keep & ~post] = plaster[top & ~keep & ~post]
# The post runs on to the ridge as it does up to the windows' top.
pz = near.lookup(np.stack([x_, q[..., 1], wv(z_, 60.0, 76.0)], -1))
near.tex[post & (z_ > 76.0)] = pz[post & (z_ > 76.0)]
# The back corner under the eave is painted in the same shadow: it takes the panel beside it.
corner = (x_ < -79.0) & (z_ > 38.0) & (z_ <= 59.0)
near.tex[corner] = near.lookup(np.stack([wv(x_, -77.0, -68.0), q[..., 1], z_], -1))[corner]

# The wall beyond the picture's end continues the seen wall, mirrored at the corner.
q = right.world()
past = q[..., 1] > WY_PIC
mirror = np.stack([q[..., 0], np.where(past, 2 * WY_PIC - q[..., 1], q[..., 1]), q[..., 2]], -1)
right.tex[:] = right.lookup(mirror)

# Hidden faces reuse their named source.
for f in faces:
    if f in SEEN_FACES or f.src is None:
        continue
    srcf, fn = f.src
    sf = NAMED[srcf] if isinstance(srcf, str) else srcf
    f.tex[:] = sf.lookup(fn(f, f.world()))
# The back wall: the seen long wall below its eave height, the near gable's upper timber band above it.
q = wall_b.world()
y, z = q[..., 1], q[..., 2]
low = np.stack([np.zeros_like(y), wv(y, PY + 3, WY_PIC - 1), wv(z, 6.0, 34.0)], -1)
wall_b.tex[:] = right.lookup(low)

# The undersides are never in the picture: a soffit in shade, one colour taken from the gable's darkest timber.
L_ = near.tex @ np.array([.299, .587, .114])
soffit = np.median(near.tex[L_ < np.percentile(L_, 25)], axis=0)
under_b.tex[:] = soffit
under_f.tex[:] = soffit
mod, FI, atlas = build()
finish('e_house01_3', BK, mod, FI, atlas, PIC, OX, OY, opaque, iou, report)
