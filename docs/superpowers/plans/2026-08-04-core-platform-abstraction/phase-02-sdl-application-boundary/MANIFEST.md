# Phase 02 — SDL Application Boundary

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Make PlatformRuntime the sole owner of SDL application state and expose fixed-layout window and event services.

| Packet | Depends on | Owns |
|---|---|---|
| P02-M01 | P01-M06 | SDL lifetime and application window |
| P02-M02 | M01 | normalized event queue |
| P02-M03 | M02 | display, resize, focus, fullscreen |
| P02-M04 | M03 | mouse, cursor, clipboard, controller |
| P02-M05 | M04 | application lifecycle integration gate |

Exit: the native SDL application contract passes with one window owner, deterministic event translation, and clean restart.

P02-M01 checkpoint: the SDLApplication lifecycle test passes natively on Windows and the private borrowed-window bridge is retained. The test target now uses the Windows CRT entry point and links the shared PlatformRuntime client correctly; desktop/GPU acceptance and full PlatformRuntime ABI ownership remain open.

P02-M02 checkpoint: the existing SDL event translator compiles on Windows with injected resize, key, text, motion, wheel, quit, and unknown-event fixtures; overflow policy and runtime execution remain open.

P02-M03 checkpoint: window display/fullscreen contracts pass natively on Windows; the former run-mode hang was caused by the test target's raw `main`/CRT wiring and is resolved. The 100-cycle desktop acceptance remains open.

P02-M04 checkpoint: input/clipboard contract compilation passes on Windows; virtual-controller name-copy/removal coverage passes, while runtime hardware/display acceptance remains open.
P02-M02 update: bounded text payload overflow now emits one platform diagnostic per episode.
P02-M04 update: controller names are copied through a caller-buffer API and virtual controller fixtures pass without hardware.
