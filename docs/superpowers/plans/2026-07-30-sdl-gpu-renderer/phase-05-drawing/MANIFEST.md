# Phase 05 — Render Passes, Pipelines, Bindings, and Draws

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Encode validated non-indexed/indexed draws for static and temporary geometry with cached immutable pipelines and explicit bindings.

**Architecture:** Draw submission derives a pipeline key from normalized state, obtains one pass compatible with active attachments, binds dirty resources/uniforms, and emits a draw. Tests separate pure planning from SDL command emission.

**Tech Stack:** SDL_GPU graphics pipelines/render passes/bindings/draw commands.

---

| Packet | Depends on | Owns |
|---|---|---|
| P05-M01 | P01-M04, P03-M05, P04-M05 | pipeline cache |
| P05-M02 | M01, P02-M03 | render-pass controller |
| P05-M03 | M02, P03-M02, P03-M04 | bindings and static draws |
| P05-M04 | M03 | indexed/temp draws and reference smoke |

Exit: deterministic smoke draws untextured and textured triangles, indexed geometry, dynamic updates, depth-tested overlap, viewport change, and offscreen-to-swapchain rendering with exact readback pixels.
