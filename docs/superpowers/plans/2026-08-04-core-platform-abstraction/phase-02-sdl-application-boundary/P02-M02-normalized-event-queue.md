# P02-M02 — Normalized Event Queue

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Append fixed-layout keyboard, text, mouse, controller, window, and quit event records to the ABI.

**Dependencies:** P02-M01.

**Allowed files:** `Sources/src/PlatformABI/platform_c.h`, `Sources/src/Platform/Event.h`, `Sources/src/Platform/SDL/Events.cpp`, `Sources/src/Platform/SDLApplication.cpp`, `tools/zig/platform_event_test.cpp`, `build.zig`.

- [x] Keep injected SDL fixtures for key, UTF-8 text, motion, wheel, resize, and quit records in the existing event test.
- [x] Compile checks cover stable event kind, timestamp, modifiers, coordinates, and bounded text payload fields.
- [x] Translate SDL events once into bounded `PlatformEvent` records in arrival order.
- [x] Define bounded text overflow behavior: truncate at the fixed payload limit and emit one platform diagnostic for each consecutive overflow episode.
- [x] Assert gameplay consumers do not include SDL event headers.
- [x] Commit checkpoint: `platform: expose normalized application events`.

**Evidence:** Windows `test-platform-events -Dtarget=x86_64-windows-msvc -Dtest-mode=compile` passes with the bounded overflow implementation; run-mode validation is coupled to the headless SDL window blocker recorded in P02-M01.
