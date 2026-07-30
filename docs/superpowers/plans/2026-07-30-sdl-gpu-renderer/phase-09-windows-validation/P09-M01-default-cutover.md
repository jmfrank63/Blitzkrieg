# P09-M01 — Make SDL_GPU the Windows Renderer

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Change the build default, remove legacy renderer compilation/linkage, and preserve reference history in Git.

**Dependencies:** P08-M04.

**Allowed files:** `build.zig`, renderer-only include/source lists in project metadata when still consumed by the Zig build.

- [ ] Add a failing link audit that reports renderer inputs containing `GraphicsEngine.cpp`, `Texture.cpp`, `GeometryBuffer.cpp`, `d3d9`, or `dxguid`.
- [ ] Change default `renderer` to `sdl_gpu`; retain `legacy` only as an explicitly named comparison build if it can remain without polluting default linkage.
- [ ] Remove D3D9/DXGUID linkage from the GFX target. Do not remove unrelated DirectInput `dxguid` or game-side D3D dependencies until the audit proves they are renderer-owned and safe.
- [ ] Remove legacy implementation sources from default GFX and ensure `Specific.h` is not imported by new renderer sources.
- [ ] Build default `gfx`, `game`, and `game-all`; run link audit.
- [ ] Commit: `build: make SDL GPU the default renderer`

**Evidence:** before/after link list and zero renderer audit findings.
