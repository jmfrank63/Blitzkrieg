# P03-M06 — Close the Input Shared-Module Gate

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Build and load the real Input module on all targets with no DirectInput dependency in the portable graph.

**Dependencies:** P03-M05.

**Allowed files:** `Sources/src/Input/GlobalsLoader.cpp`, `Sources/src/Input/StdAfx.h`, `Sources/src/Input/InputObjectFactory.cpp`, `Sources/src/Input/Input.def`, `tools/zig/input_module_test.cpp`, `tools/zig/runtime_platform_audit.zig`, `build.zig`.

- [ ] Load the real Input factory, initialize it against PlatformRuntime, inject the full fixture corpus, and shut it down twice.
- [ ] Target-guard `.def`, import libraries, COM support, DirectInput libraries, and Windows-only oracle source files.
- [ ] Run `zig build test-input-module -Dtest-mode=run` natively on Windows and Linux; compile for macOS.
- [ ] Verify link commands contain no `dinput8`, `dxguid`, `winmm`, or `user32` on Linux/macOS.
- [ ] Remove all Input-owned native tokens from the audit allowlist.
- [ ] Commit: `input: close portable module gate`

**Evidence:** factory/lifecycle output and three-target link audit.
