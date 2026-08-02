# Phase 04 — Input and Audio

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Replace DirectInput/Win32 input with translated SDL events and make the existing miniaudio backend lifecycle independent of Windows handles/threads.

**Architecture:** Engine-neutral events feed `CInputAPI`; legacy control IDs remain stable. miniaudio remains the sole mixer/device backend and uses portable worker primitives.

**Tech Stack:** SDL3 keyboard/mouse/text/clipboard/gamepad APIs, miniaudio, Phase 01 clock/sync.

---

| Packet | Depends on | Owns | Gate |
|---|---|---|---|
| P04-M01 | P03-M05 | stable input codes/device catalog | mapping tests |
| P04-M02 | M01 | keyboard/text/focus | keyboard tests |
| P04-M03 | M01 | mouse/wheel/cursor | mouse tests |
| P04-M04 | M02, M03 | clipboard/controller and input integration | input lifecycle |
| P04-M05 | M02, P03-M05 | portable miniaudio initialization and window-free runtime init ABI | audio smoke |
| P04-M06 | M04, M05 | audio workers and phase gate | input/audio native gate |

Phase exit: `test-platform-input` and `test-platform-audio` natively on Windows/Linux; macOS compile.
