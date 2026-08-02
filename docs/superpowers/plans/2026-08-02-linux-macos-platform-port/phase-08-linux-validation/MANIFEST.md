# Phase 08 — Linux Runtime and Human Acceptance

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Package and accept the complete game on native x86_64 Linux using SDL_GPU/Vulkan.

**Architecture:** A portable directory contains the executable, `.so` modules, SDL runtime, backend shaders, and read-only Data. A Zig smoke controller handles automatic launch/log/timeout; human packets verify playability.

**Tech Stack:** x86_64 Linux GNU target, SDL3, Vulkan SDL_GPU driver, SPIR-V, Zig stage/smoke tools.

---

| Packet | Depends on | Owns | Gate |
|---|---|---|---|
| P08-M01 | P07-M05 | Linux stage/package layout | runtime layout verifier |
| P08-M02 | M01 | automatic startup/reference/save smoke | `test-game-smoke` |
| P08-M03 | M02 | lifecycle/endurance automation | endurance test |
| P08-M04 | M03 | menu/mission human verification package | reviewer checklist |
| P08-M05 | M04 | accepted Linux evidence | Linux acceptance |

Phase exit: human-approved `linux-acceptance.md` with all automated commands green.
