# Phase 07 — Backend-Neutral Shader Artifacts

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Produce deterministic DXIL, SPIR-V, and MSL from one HLSL catalog and load the format required by SDL_GPU's selected driver.

**Architecture:** A multi-format manifest records one entry per effect/stage/format. Zig-built shadercross runs offline; runtime validates and creates shaders only from the selected format.

**Tech Stack:** canonical HLSL, Zig-built SDL_shadercross, SDL_GPU shader formats, SHA-256 manifests.

---

| Packet | Depends on | Owns | Gate |
|---|---|---|---|
| P07-M01 | P06-M05 | manifest schema v3 | parser tests |
| P07-M02 | M01 | deterministic SPIR-V outputs | Linux shader corpus |
| P07-M03 | M01, M02 | deterministic MSL outputs | macOS shader corpus |
| P07-M04 | M03 | runtime format selection | loader/device tests |
| P07-M05 | M04 | native renderer smoke matrix | three-driver gate |

Phase exit: shader determinism passes and native Windows/Linux/macOS smoke selects DXIL/SPIR-V/MSL respectively.
