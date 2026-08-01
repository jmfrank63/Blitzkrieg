# Phase 01 — Pure Renderer Core

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Model handles, legacy formats, vertex layouts, mutable state, immutable pipeline keys, and coordinate conversion without requiring a GPU.

**Architecture:** All legacy numeric values are validated at the renderer boundary and converted to compact Zig values. Equality/hash tests make pipeline cache behavior deterministic.

**Tech Stack:** Zig unit tests and legacy definitions in `GFXTypes.h`.

---

| Packet | Depends on | Owns |
|---|---|---|
| P01-M01 | P00-M04 | diagnostics and generational registries |
| P01-M02 | M01 | format/topology conversion |
| P01-M03 | P01-M02 | FVF vertex-layout decoding |
| P01-M04 | P01-M03 | normalized render state and pipeline keys |
| P01-M05 | P01-M04 | matrix/color/viewport conversion |

Exit: `zig build test-gfxgpu-core` passes, including exhaustive known-value tables from `GFXTypes.h`.
