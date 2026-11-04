"""The paint templates' camera, canvas and file, on a tracked hand model and without Blender: python -I tools/blender/test_blitzkrieg_views.py"""
import os, sys, tempfile, zipfile
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.dont_write_bytecode = True
import numpy as np
import blitzkrieg_views as v

ROOT = os.path.join(HERE, '..', '..')
tri, uv, doc = v.load_hand(os.path.join(ROOT, 'tools/zig/fixtures/bk-views/hand-models/e_house01_1.json'))
# The 2003 camera: a step along x goes right and down, height goes straight up.
assert np.allclose(v.project(np.array([1.0, 0, 0])), [0.7071, 0.3536], atol=1e-3)
assert np.allclose(v.project(np.array([0, 0, 1.0])), [0, -0.8660], atol=1e-3)
assert np.allclose(v.quarter(np.array([1.0, 0, 0]), 1), [0, 1, 0])
for k in range(4):
    x0, y0, W, H = v.canvas(tri, k)
    assert x0 < 0 < x0 + W and y0 < 0 < y0 + H, 'the zero cross lies on the canvas'
    s = v.project(v.quarter(tri, k)).reshape(-1, 2)
    assert (s >= [x0, y0]).all() and (s <= [x0 + W, y0 + H]).all()
    layers = v.view_layers(tri, uv, [], np.zeros(len(tri), int), k)
    assert layers['opaque'].any() and layers['wire'][..., 3].any() and 'texture' not in layers
    light = layers['light'][layers['opaque']][:, 0]
    assert light.min() < light.max() <= 255, 'faces differ in light'
# A view lines up with the picture it was measured on: the outline at step 0 against the 2003 picture.
sys.path.insert(0, os.path.join(ROOT, 'tools/house_model'))
import bk1
sprite = bk1.pack(os.path.join(ROOT, 'Data/Buildings/europe/summer/e_house01_1'), '1')[0]
layers = v.view_layers(tri, uv, [], np.zeros(len(tri), int), 0)
picture = v.laid(sprite, layers['box'])[..., 3] >= 128
overlap = (picture & layers['opaque']).sum() / (picture | layers['opaque']).sum()
assert overlap > 0.9, overlap
# Another side's picture finds its place by its outline, and OpenRaster wants its mimetype first and unpacked.
with tempfile.TemporaryDirectory() as d:
    layout = v.write_templates(tri, uv, [], np.zeros(len(tri), int), os.path.join(ROOT, 'Data/Buildings/europe/summer/e_house01_1'), d, lambda s: None, 2)
    x0, y0, W, H = v.canvas(tri, 2)
    assert layout['step_2'] == dict(scale=2, size=[W * 2, H * 2], zero=[-x0 * 2, -y0 * 2])
    with zipfile.ZipFile(os.path.join(d, 'step_2.ora')) as z:
        first = z.infolist()[0]
        assert first.filename == 'mimetype' and first.compress_type == zipfile.ZIP_STORED
        stack = z.read('stack.xml').decode()
        assert 'e_house01_2 state 3' in stack and 'svg:multiply' in stack
# An enlarged canvas: the model's layers grow and stay on the same outline, an enlarged picture keeps the Game's pixels at its block middles.
big = v.view_layers(tri, uv, [], np.zeros(len(tri), int), 0, 2, 3)
assert big['wire'].shape[:2] == (layers['box'][3] * 3, layers['box'][2] * 3)
assert (big['opaque'] != layers['opaque']).mean() < 0.01
assert big['zero cross'][-layers['box'][1] * 3, -layers['box'][0] * 3, 3] == 255
shown = v.laid(sprite, layers['box'])
solid = shown[..., 3] == 255
inner = solid.copy()
for r in (1, 2, 3):
    inner &= np.roll(solid, r, 0) & np.roll(solid, -r, 0) & np.roll(solid, r, 1) & np.roll(solid, -r, 1)
back = v.enlarge(shown, 3)[1::3, 1::3]
assert np.abs(back[inner][:, :3].astype(float) - shown[inner][:, :3]).max() <= 1
large = v.enlarge(shown, 4)
assert large.shape[:2] == (shown.shape[0] * 4, shown.shape[1] * 4)
# Nothing but the outline's own soft edge: no ripple of faint pixels further out, and no dark or wild colour in its edge.
near = shown[..., 3] > 0
for _ in range(2):
    near = near | np.roll(near, 1, 0) | np.roll(near, -1, 0) | np.roll(near, 1, 1) | np.roll(near, -1, 1)
assert not large[..., 3][~np.kron(near, np.ones((4, 4), bool))].any()
# A fixture's corners are shared on import, at the same places and in the same order, which is what its texture coordinates rely on.
points, corners = v.welded(tri)
assert len(points) < tri.size // 3 and (points[corners] == tri).all()
assert all(len(set(c)) == 3 for c in corners.tolist())
print('ok, step 0 outline against the 2003 picture', round(float(overlap), 3))
