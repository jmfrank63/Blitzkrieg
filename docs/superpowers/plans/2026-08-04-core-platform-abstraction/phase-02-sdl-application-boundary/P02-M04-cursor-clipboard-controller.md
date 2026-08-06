# P02-M04 — Cursor, Clipboard, and Controller Services

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Expose mouse capture/warp/visibility, UTF-8 clipboard, and controller discovery through SDL services.

**Dependencies:** P02-M03.

**Allowed files:** `Sources/src/PlatformABI/platform_c.h`, `Sources/src/Platform/SDL/Cursor.cpp`, `Sources/src/Platform/SDL/Clipboard.cpp`, `Sources/src/Platform/SDL/Controller.cpp`, `tools/zig/platform_input_test.cpp`, `tools/zig/platform_clipboard_test.cpp`, `build.zig`.

- [x] Existing Windows compile contract covers cursor/clipboard/controller service linkage; runtime hardware/display execution remains open.
- [x] Keep cursor and clipboard operations owned by the SDL application facade.
- [x] Copy controller names into caller buffers with truncation status and add virtual-controller fixtures.
- [x] Inject virtual-controller fixtures so CI does not require hardware.
- [x] Compile the input/clipboard gate on Windows; native runtime execution remains tied to the headless SDL blocker.
- [x] Commit checkpoint: `platform: expose cursor clipboard and controllers`.

**Evidence:** `test-platform-controller -Dtarget=x86_64-windows-msvc -Dtest-mode=run` passes virtual name copy, short-buffer truncation, duplicate rejection, and removal without hardware; SDL runtime hardware/display evidence remains open.
