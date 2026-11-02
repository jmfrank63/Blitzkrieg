"""A hand model written for bk-views (tools/zig/bk_views_hand.zig): its faces as JSON and its atlas as the PNG bk-views reads
(8-bit RGBA, one IDAT, no row filters). They are tracked, so the build needs no Python."""
import json, os, struct, zlib
import numpy as np

DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'zig', 'fixtures', 'bk-views', 'hand-models')


def group_of(name):
    """A face's part and look, as the review's meshes name them: a damage state's fallen roof goes by the `.roof` look."""
    if name.startswith('underside'):
        return 'roof.under'
    if name.startswith(('roof', 'verge', 'eave')):
        return 'roof.roof'
    if name.startswith('dormer'):
        return 'dormer.roof' if name.endswith('roof') else 'dormer.wall'
    if name.startswith(('chimney', 'collar', 'pot')):
        return 'chimney.roof' if name.endswith('top') else 'chimney.wall'
    if name.startswith('post'):
        return 'porch.wall'
    if name.startswith('porch floor'):
        return 'porch.roof'
    return 'body.wall'


def png(rgb):
    h, w = rgb.shape[:2]
    rgba = np.dstack([rgb, np.full((h, w), 255, np.uint8)])
    raw = np.zeros((h, w * 4 + 1), np.uint8)
    raw[:, 1:] = rgba.reshape(h, w * 4)

    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw.tobytes(), 9)) + chunk(b'IEND', b'')


def write(name, faces, mod, FI, atlas, state='1'):
    """`mod`, `FI`, `atlas` as the model's build() returns them; `faces` in FI's order."""
    os.makedirs(DIR, exist_ok=True)
    H, W = atlas.shape[:2]
    groups = sorted({group_of(f.name) for f in faces})
    uv = mod['UV'] * np.array([W, H])
    doc = dict(width=W, height=H, groups=groups,
               charts=[[f.ax - 3, f.ay - 3, f.w + 6, f.h + 6, groups.index(group_of(f.name))] for f in faces],
               vertices=[[round(float(v), 4) for v in (*p, *t)] for p, t in zip(mod['P'], uv)],
               triangles=[[int(t[0]), int(t[1]), int(t[2]), int(i)] for t, i in zip(mod['T'], FI)],
               normals=[[round(float(v), 6) for v in f.n] for f in faces])
    with open(os.path.join(DIR, name + '.json'), 'w', newline='\r\n') as o:
        json.dump(doc, o, separators=(',', ':'))
        o.write('\n')
    with open(os.path.join(DIR, f'{name}_{state}.png'), 'wb') as o:
        o.write(png(atlas))
