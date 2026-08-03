# Windows native renderer regression

Date: 2026-08-03

`zig build gfxgpu-shaders -Dshader-formats=dxil,spirv,msl` followed by `zig build gfxgpu-smoke` passed on the native Windows host.

- SDL selected `direct3d12` exactly as requested.
- The renderer selected DXIL from the multi-format manifest.
- Reference, textured, pixel-transform, and depth checks passed.
- 25 shader pairs, 25 probe geometries, and 25 pipelines were created and released.
- Three reference frames produced identical SHA-256 hashes.
