# Phase 09 — Windows 11 x64 Cutover and Handoff

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Make SDL_GPU the single Windows renderer, package its runtime/shaders, prove the complete acceptance gate, and leave a precise native Linux/macOS platform-port handoff.

**Architecture:** The cutover removes legacy D3D renderer objects and renderer-specific D3D linkage. SDL_GPU remains backend-neutral and selects Direct3D 12 on Windows.

**Tech Stack:** Zig build/staging/package tools, SDL3 runtime, DXIL blobs, Windows 11 x64.

---

| Packet | Depends on | Owns |
|---|---|---|
| P09-M01 | P08-M04 | default cutover and D3D renderer unlink |
| P09-M02 | M01 | runtime/shader staging and verification |
| P09-M03 | M02 | complete Windows acceptance run |
| P09-M04 | M03 | portability audit and successor handoff |

Exit: all final commands pass, real game acceptance is recorded, no renderer D3D9 dependency remains, and the portability audit contains no backend-native renderer calls.
