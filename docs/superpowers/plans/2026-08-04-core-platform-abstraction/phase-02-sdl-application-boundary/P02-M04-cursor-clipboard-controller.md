# P02-M04 — Cursor, Clipboard, and Controller Services

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Expose mouse capture/warp/visibility, UTF-8 clipboard, and controller discovery through SDL services.

**Dependencies:** P02-M03.

**Allowed files:** `Sources/src/PlatformABI/platform_c.h`, `Sources/src/Platform/SDL/Cursor.cpp`, `Sources/src/Platform/SDL/Clipboard.cpp`, `Sources/src/Platform/SDL/Controller.cpp`, `tools/zig/platform_input_test.cpp`, `tools/zig/platform_clipboard_test.cpp`, `build.zig`.

- [ ] Test capture acquire/release, relative mode, warp, visibility nesting, focus loss, clipboard empty/non-ASCII text, hotplug, axes, buttons, and stale controller handles.
- [ ] Implement one owner for cursor state and restore state during runtime destruction.
- [ ] Copy clipboard and controller names into caller buffers.
- [ ] Inject virtual-controller fixtures so CI does not require hardware.
- [ ] Run input and clipboard tests natively on Windows and Linux.
- [ ] Commit: `platform: expose cursor clipboard and controllers`

**Evidence:** cursor transition log, UTF-8 clipboard fixture, controller event fixture.
