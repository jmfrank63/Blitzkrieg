# Phase 03 — SDL Application and Window Lifecycle

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Give SDL3 sole ownership of application lifetime, the game window, and event pumping while the existing game and renderer consume portable boundaries.

**Architecture:** A common `GameMain` contains game startup. `SDLApplication` owns SDL/window state and emits SDL-free events. The renderer borrows the owned `SDL_Window *` and releases GPU ownership before window destruction.

**Tech Stack:** SDL3 application/window/event APIs, C++17 game shell, existing SDL_GPU ABI.

---

| Packet | Depends on | Owns | Gate |
|---|---|---|---|
| P03-M01 | P02-M06 | portable entry and arguments | command-line tests |
| P03-M02 | M01 | SDL init/window owner | hidden window lifecycle |
| P03-M03 | M02 | event translation and WinFrame bridge | synthetic event tests |
| P03-M04 | M02 | borrowed renderer window | GPU factory/smoke |
| P03-M05 | M03, M04 | display/fullscreen/bootstrap gate | native window gate |

Phase exit: native `test-platform-window` plus Windows `test-gfxgpu` and Linux compile.
