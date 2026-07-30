# P06-M03 — Adapt Textures, Surfaces, and Render Targets

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Implement engine texture/surface objects as metadata plus Zig handles.

**Dependencies:** P06-M02, P03-M03, P03-M05.

**Allowed files:** `Sources/src/GFXGPU/TextureGpu.h`, `Sources/src/GFXGPU/TextureGpu.cpp`, `Sources/src/GFXGPU/GraphicsEngineGpu.cpp`, `tools/zig/gfxgpu_adapter_test.cpp`.

- [x] Inventory `IGFXTexture` and surface virtual methods from `GFX.H`; assert every method is overridden.
- [x] Add fake-API tests for create from description, upload each mip, bind stages 0/1, refcount copy/release, render-target creation/bind/restore, stale use, and failure rollback.
- [x] Store parent `GraphicsEngineGpu*`, one handle, dimensions/format/mips/usage, and refcount; store no pixel allocation or SDL pointer after upload.
- [x] Preserve lock/unlock behavior by using adapter-owned temporary CPU bytes only between lock and unlock; unlock uploads and releases the temporary allocation.
- [x] Reject simultaneous double lock and unsupported read lock with legacy-compatible failure.
- [x] Release the Zig handle exactly once before adapter deletion.
- [x] Commit: `feat: adapt IGFX texture and target objects`

**Evidence:** virtual method inventory and release-once tests.
