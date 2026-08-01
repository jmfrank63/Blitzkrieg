# Phase 02 — SDL_GPU Device and Frame Lifecycle

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Create D3D12 through SDL_GPU, claim an SDL window, record a valid clear/present frame, handle resize/minimize, and shut down cleanly.

**Architecture:** `sdl.zig` is the sole SDL import boundary. `Device`, `Surface`, and `Frame` have explicit ownership and state machines.

**Tech Stack:** SDL3 3.2+ GPU API, Direct3D 12 SDL driver, Zig.

---

| Packet | Depends on | Owns |
|---|---|---|
| P02-M01 | P00-M05, P01-M05 | SDL façade and device |
| P02-M02 | M01 | window claim and swapchain |
| P02-M03 | M02 | frame/command-buffer state machine |
| P02-M04 | M03 | clear/present, resize, minimize, restart |

Exit: forced `direct3d12` smoke renders 120 frames, programmatically resizes, simulates a minimized interval, restores, and exits with zero validation errors.
