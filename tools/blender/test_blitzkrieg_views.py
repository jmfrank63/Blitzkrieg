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
    v.write_templates(tri, uv, [], np.zeros(len(tri), int), os.path.join(ROOT, 'Data/Buildings/europe/summer/e_house01_1'), d, lambda s: None)
    with zipfile.ZipFile(os.path.join(d, 'step_2.ora')) as z:
        first = z.infolist()[0]
        assert first.filename == 'mimetype' and first.compress_type == zipfile.ZIP_STORED
        stack = z.read('stack.xml').decode()
        assert 'e_house01_2 state 3' in stack and 'svg:multiply' in stack
print('ok, step 0 outline against the 2003 picture', round(float(overlap), 3))
