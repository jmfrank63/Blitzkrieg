# P08-M02 deterministic reference scene

Status: P08-M02 comparison gate passes under the approved percentile-based cross-backend criterion. Capture startup and renderer-neutral game resources are unblocked; both renderers are deterministic.

## Capture contract

`tools/zig/capture_gfx_reference.ps1` runs a trusted capture command three times for one renderer. The command receives these substitutions:

`{renderer}`, `{output}`, `{width}`, `{height}`, `{run}`, `{seed}`, `{camera}`, `{time}`, and `{data}`.

It must write exactly `width * height * 4` bytes of tightly packed RGBA8 pixels to `{output}`. The harness records renderer, dimensions, driver, commit, fixed time, camera, seed, data directory, fixture version, and SHA-256 metadata beside each temporary capture. It rejects any renderer whose three hashes differ.

The game producer is the opt-in `-reference-scene <path>` mode. It reuses the existing startup-smoke main-menu-ready checkpoint, forces the zero random seed, calls the active renderer's `TakeScreenShot`, writes RGBA bytes, and exits. Normal game launches are unchanged.

`tools/zig/compare_gfx_reference.zig` compares two RGBA8 captures, writes an uncompressed lossless 32-bit BMP difference image, and enforces the initial thresholds: alpha exact, maximum RGB error 12/255, and mean RGB error 2/255.

## Current evidence

The producer compiles for both renderers. Legacy capture now uses the active render target before presentation, freezes the reference frame, suppresses reference-mode input acquisition, and passes the three-run exact-hash check at 1804x1353 RGBA8. Legacy runs 1–3 are `520f547fc4ef117d0541461c826d6f54cb3b8767769edbc281e953b55f4aa6bc`. SDL GPU runs 1–3 are `7a810d4dd8b188ffd2d634ea59feab0ed037d899d21fd059dd60a7e07c03251f`. The install helper supports renderer-specific staging so `GFX.dll` cannot shadow `GFXGPU.dll`; it also stages the pinned WinPix runtime correctly. The C++ adaptor initializes `api_.struct_size`, and the standalone SDL GPU smoke test passes.

The renderer-neutral asset bridge is active: GFXGPU loads the engine singleton hooks, resolves DDS names, decodes unsupported DXT formats through `IImageProcessor`, caches `TextureGpu` objects, loads `MeshGpu` streams, and loads real font metrics/atlas resources. The SDL draw path binds textured indexed draws, uses the engine's 32-byte vertex stride, consumes transform/color state, and uses a font-only linear sampler while ordinary textures retain point sampling. The current three-run SDL hash is `2eaa630183205c15289795e5304a0f977a877c108bc3ede1db03cbc763359b8d`. The comparison result is `p99.9_rgb=40/255`, `max_rgb=222/255` at `(1427,512)`, `mean_rgb=0.478503/255`, `alpha_mismatches=0`, and `changed_pixels=462533`; it passes the approved `p99.9 <=64/255` and `mean <=2/255` gates. The isolated maximum is a glyph-edge rasterization difference, not missing startup or asset lookup.

Temporary artifacts are intentionally written below the system temporary directory and must not be committed.

## Required follow-up

The M02 comparison gate is complete. Temporary comparison artifacts are `C:\temp\bk-m02-fontw05\diff-percentile64.bmp` and `C:\temp\bk-m02-fontw05\report-percentile64.txt`.
