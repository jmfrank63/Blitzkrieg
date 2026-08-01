# P02-M01 — Create the SDL_GPU Device

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Wrap SDL imports and create/release a validated GPU device.

**Dependencies:** P00-M05, P01-M05.

**Allowed files:** `Sources/src/GFXGPU/sdl.zig`, `Sources/src/GFXGPU/device.zig`, `Sources/src/GFXGPU/renderer.zig`, `Sources/src/GFXGPU/root.zig`, `build.zig`.

**Creation call:** `SDL_CreateGPUDevice(format_flags, debug_mode, preferred_driver)`; Windows smoke passes `SDL_GPU_SHADERFORMAT_DXIL` and `"direct3d12"`.

- [ ] Add tests around an injected `DeviceApi` fake: create failure captures SDL text; unsupported driver fails; release occurs once; partial init releases.
- [ ] Make `sdl.zig` import `SDL3/SDL.h` and expose only renderer-needed aliases/wrappers.
- [ ] Implement `Device.init` and `deinit`, record driver name and supported shader format, and enable SDL debug mode from the build/create flag.
- [ ] Add API accessors for startup log fields; no native device pointer may leave this module family.
- [ ] Extend smoke to create/release the device without a claimed window.
- [ ] Force `SDL_GPU_DRIVER=direct3d12` and run smoke.
- [ ] Commit: `feat: create SDL GPU device`

**Evidence:** startup driver line and fake failure-path tests.
