# P01-M06 — Environment, Dialog, Launch, and Process Services

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Complete the core system boundary for environment values, executable path, dialogs, URLs/files, and child processes.

**Dependencies:** P01-M05.

**Allowed files:** `Sources/src/PlatformABI/platform_c.h`, `Sources/src/Platform/Core/System.cpp`, `Sources/src/Platform/Windows/System.cpp`, `Sources/src/Platform/Posix/System.cpp`, `Sources/src/Platform/Linux/System.cpp`, `Sources/src/Platform/MacOS/System.cpp`, `Sources/src/Platform/System.h`, `Sources/src/Platform/System.cpp`, `tools/zig/platform_system_test.cpp`, `build.zig`.

- [ ] Test absent/present environment values, executable path, noninteractive dialog capture, URL validation, argument preservation, exit code, timeout, and child output limits.
- [ ] Implement native backends with no shell command construction.
- [ ] Add injectable test callbacks so CI never opens a real browser or modal dialog.
- [ ] Convert the existing `System` facade and preserve UTF-8 diagnostics.
- [ ] Run `zig build test-platform-system -Dtest-mode=run` natively on Windows and Linux and compile for macOS.
- [ ] Commit: `platform: complete core system services`

**Evidence:** exact argument vector, exit code, and captured dialog records.
