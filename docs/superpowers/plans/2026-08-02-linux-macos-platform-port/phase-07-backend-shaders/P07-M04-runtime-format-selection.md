# P07-M04 — Select Runtime Shader Format by SDL_GPU Driver

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Advertise available formats and load only the artifact matching the selected SDL_GPU backend.

**Dependencies:** P07-M03.

**Allowed files:** `Sources/src/GFXGPU/device.zig`, `Sources/src/GFXGPU/shader_manifest.zig`, `Sources/src/GFXGPU/shaders.zig`, `Sources/src/GFXGPU/renderer.zig`, `Sources/src/GFXGPU/gfxgpu_c.h`, `tools/zig/gfxgpu_smoke.zig`.

- [ ] Add pure tests mapping `direct3d12->dxil`, `vulkan->spirv`, `metal->msl`, rejecting unknown/unsupported requested combinations and missing records.
- [ ] Create the SDL GPU device with all packaged format flags and an optional preferred driver; query/log the selected driver and choose one manifest format.
- [ ] Generalize shader creation info for DXIL/SPIR-V/MSL while retaining hash/length/binding validation and paired rollback.
- [ ] Fail startup with driver, required format, effect/stage, and missing path when a package is incomplete.
- [ ] Run core/compatibility tests and build smoke for all targets.
- [ ] Commit: `gfx: select backend shader artifacts at runtime`

**Evidence:** driver-format mapping tests and missing-artifact diagnostic.
