# P06-M01 — Build the Adapter DLL and Factory Shell

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Produce a selectable GFX DLL whose factory returns an `IGFX` object backed by the versioned API table.

**Dependencies:** P00-M02, P05-M04.

**Allowed files:** `build.zig`, `Sources/src/GFXGPU/GFXGPU.def`, `Sources/src/GFXGPU/GfxGpuObjectFactory.cpp`, `Sources/src/GFXGPU/GraphicsEngineGpu.h`, `Sources/src/GFXGPU/GraphicsEngineGpu.cpp`, `tools/zig/gfxgpu_factory_test.cpp`.

- [x] Inventory exact factory exports from `Sources/src/GFX/GFX.def` and object-factory source; add a test that loads/calls the same exported names.
- [x] Define `GraphicsEngineGpu final : public IGFX` with all overrides declared; methods not assigned until later packets return the existing engine failure convention and set an adapter diagnostic.
- [x] Load `GfxGpuApi` once in the constructor path, verify version/size, and fail factory creation cleanly on mismatch.
- [x] Add `addGFXGPU` build function with only adapter sources, `GfxGpuZig`, SDL3, and existing non-D3D support libraries.
- [x] Keep `-Drenderer=legacy` unchanged; map `sdl_gpu` to the new artifact.
- [x] Run factory test and both renderer `gfx` builds.
- [x] Commit: `feat: add SDL GPU IGFX adapter shell`

**Evidence:** export list, API size, and source/link list proving no legacy implementation object is included.
