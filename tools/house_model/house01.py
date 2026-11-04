"""The body and roof the e_house01 relatives share, as measured on e_house01_1's picture."""
# The roof profile, measured on the picture: apex, front eave and back eave.
# The roof runs -2 to 112: the best outline against the picture, with the dormers and chimney where they are. A roof at -10 to 107
# read as set too far forward over the picture (the maintainer, 2026-11-03). Built and read with the same geometry.
PIC_Y0, PIC_Y1 = -2.0, 112.0
ROOF_Y0, ROOF_Y1 = PIC_Y0, PIC_Y1
XE, XR, XB = 3.0, -67.7, -105.3
ZE, ZR, ZBE = 38.0, 108.0, 42.9
SLAB = 3.0
WX = -92.0  # back wall
# The picture ends the long wall at 89.5, which leaves the roof 18.5 past it at this end and only 3 at the near one. Turned,
# that reads as a roof slid backwards and a body too short (the maintainer, 2026-11-02), so the body runs on to 100.
WY_PIC, WY = 89.5, 100.0
PX, PY = -27.0, 35.0  # porch corner


def zf(x):
    return ZE + (ZR - ZE) * (XE - x) / (XE - XR)


def zb(x):
    return ZR - (ZR - ZBE) * (XR - x) / (XR - XB)


def zroof(x):
    return zf(x) if x >= XR else zb(x)


def profile(y, x0, x1, z0=0.0):
    """The wall under the roof line at plane y, from x0 to x1."""
    xs = [x for x in (x0, XR, x1) if min(x0, x1) <= x <= max(x0, x1)]
    xs = sorted(set(xs))
    return [(xs[0], y, z0), (xs[-1], y, z0)] + [(x, y, zroof(x)) for x in reversed(xs)]


def slope_quad(x0, x1, y0, y1):
    return [(x0, y0, zf(x0)), (x1, y0, zf(x1)), (x1, y1, zf(x1)), (x0, y1, zf(x0))]
