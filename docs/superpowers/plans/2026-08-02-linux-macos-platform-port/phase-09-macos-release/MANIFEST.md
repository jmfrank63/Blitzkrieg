# Phase 09 — Apple-Silicon macOS and Release Validation

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Package and accept the game on native Apple-Silicon macOS, then enforce the supported three-platform release matrix.

**Architecture:** Zig assembles an unsigned `.app` bundle with relative dylib placement and read-only resources. SDL handles app/window lifecycle; native automation and a human gate precede CI/docs/final Windows regression.

**Tech Stack:** aarch64 macOS SDK/sysroot, SDL3/Metal, MSL, Zig bundle/package tools, native macOS runner.

---

| Packet | Depends on | Owns | Gate |
|---|---|---|---|
| P09-M01 | P08-M05 | `.app` bundle layout | bundle verifier |
| P09-M02 | M01 | macOS lifecycle/high-DPI integration | native window gate |
| P09-M03 | M02 | automatic startup/save smoke | native smoke |
| P09-M04 | M03 | mission/endurance plus human package | macOS UAT ready |
| P09-M05 | M04 | accepted macOS evidence | macOS acceptance |
| P09-M06 | M05 | CI/docs/final Windows/release gate | milestone complete |

Phase exit: native macOS acceptance and final Windows/Linux/macOS release checklist are green.
