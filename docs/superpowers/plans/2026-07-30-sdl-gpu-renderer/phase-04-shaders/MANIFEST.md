# Phase 04 — Offline Shader Toolchain and Baseline Shaders

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Compile canonical HLSL offline into deterministic SDL_GPU shader blobs and load validated shader pairs at runtime.

**Architecture:** SDL_shadercross is a pinned build-time tool. Runtime packages contain blobs and a generated manifest, not compiler DLLs. Windows acceptance emits DXIL; the manifest schema also names SPIR-V and MSL outputs for successor platform builds.

**Tech Stack:** HLSL, SDL_shadercross CLI, DXC/DXIL, Zig build tools.

---

| Packet | Depends on | Owns |
|---|---|---|
| P04-M01 | P00-M01 | pinned shadercross tool/dependencies |
| P04-M02 | M01 | source manifest and deterministic compiler driver |
| P04-M03 | M02, P02-M01 | runtime manifest and shader ownership |
| P04-M04 | M03, P01-M03 | shared binding/uniform contract |
| P04-M05 | M04 | untextured/textured baseline shader pairs |

Exit: clean build emits identical DXIL outputs twice, runtime creates/releases every baseline shader, and malformed manifests/blobs fail with precise diagnostics.
