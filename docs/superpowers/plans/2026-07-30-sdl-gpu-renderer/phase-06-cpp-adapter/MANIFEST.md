# Phase 06 — Thin C++ `IGFX` Adapter

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Implement Enigma's existing graphics interfaces with C++ objects that delegate all GPU ownership and draw work to the Zig C ABI.

**Architecture:** Adapter classes preserve C++ virtual layouts and engine behavior. They own refcounts, immutable metadata, and Zig handles only. `GraphicsEngineGpu` is the sole adapter entry to the API table.

**Tech Stack:** C++17, existing `GFX.H` interfaces/object factory, `gfxgpu_c.h`.

---

| Packet | Depends on | Owns |
|---|---|---|
| P06-M01 | P00-M02, P05-M04 | DLL/factory/API-table shell |
| P06-M02 | M01 | lifecycle/display/frame/state adapter |
| P06-M03 | M02, P03-M03, P03-M05 | texture/surface/target adapters |
| P06-M04 | M03, P03-M02 | vertex/index/temp geometry adapters |
| P06-M05 | M04 | draw/mesh/effect delegation |
| P06-M06 | M05 | text/rect/screenshot/gamma/stats and selectable build |

Exit: `GFXGPU.dll` builds, factory/ABI tests pass, and `game-all -Drenderer=sdl_gpu` links without legacy GFX implementation objects.
