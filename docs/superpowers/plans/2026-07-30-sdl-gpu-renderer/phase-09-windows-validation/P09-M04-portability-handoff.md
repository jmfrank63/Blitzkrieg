# P09-M04 — Audit Backend Neutrality and Write the Platform Handoff

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Prove renderer source neutrality and document the remaining non-renderer OS work for native Linux/macOS plans.

**Dependencies:** P09-M03 with accepted result.

**Allowed files:** `docs/superpowers/evidence/sdl-gpu/portability-audit.md`, `docs/superpowers/plans/2026-07-30-sdl-gpu-renderer/NEXT.md`.

- [ ] Search new renderer source/build inputs for D3D/DXGI/Vulkan/Metal/Win32/Cocoa/X11 native API types/functions and classify every hit.
- [ ] Require zero direct backend graphics API calls; HLSL/DXIL filenames, SDL driver strings, documentation, and test-only forced driver values are allowed classifications.
- [ ] Verify no SDL opaque pointer appears in `gfxgpu_c.h` except the deliberately opaque borrowed `void* sdl_window`.
- [ ] Attempt compile-only Zig renderer/core targets for `x86_64-linux` and `aarch64-macos`; record toolchain/platform blockers without weakening Windows acceptance.
- [ ] Write `NEXT.md` with remaining window/bootstrap, input, audio, filesystem, dialogs/process, networking, packaging, shader formats (SPIR-V/MSL), and native platform acceptance work.
- [ ] Include exact first successor milestone: SDL3 platform bootstrap that opens the existing game window and runs renderer smoke on native Linux, then Apple Silicon.
- [ ] Commit: `docs: hand off native platform migration`

**Evidence:** classified search, compile-only results, and successor scope.
