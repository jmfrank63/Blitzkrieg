# P08-M03 — Validate Linux Lifecycle and Endurance

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Prove native Linux survives repeated window, renderer, input, audio, module, and save lifecycle transitions.

**Dependencies:** P08-M02.

**Allowed files:** `tools/zig/game_endurance.zig`, `Sources/src/Game/GameMain.cpp`, `build.zig`, `docs/superpowers/evidence/platform-port/linux-acceptance.md`.

- [ ] Automate 100 resize events, 25 minimize/restore cycles, 10 windowed/fullscreen cycles, menu-to-mission-to-menu, audio stop/restart, focus loss/input release, save/load, and five complete process restarts.
- [ ] Bound each phase, sample resident memory and renderer/module/audio live counters at stable checkpoints, and reject monotonic growth beyond documented caches.
- [ ] Run under X11 and Wayland sessions when both are available; record one as not available rather than claiming it.
- [ ] Require normal exit, reverse module unload, joined workers, zero renderer resources, and no validation errors.
- [ ] Commit: `test: validate Linux game endurance`

**Evidence:** cycle counts, memory/counter samples, session type, and shutdown lines.
