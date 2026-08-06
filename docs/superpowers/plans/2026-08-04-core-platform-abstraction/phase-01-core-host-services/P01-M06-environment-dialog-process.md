# P01-M06 — Environment, Dialog, Launch, and Process Services

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Complete the core system boundary for environment values, executable path, dialogs, URLs/files, and child processes.

**Dependencies:** P01-M05.

**Allowed files:** `Sources/src/PlatformABI/platform_c.h`, `Sources/src/Platform/Core/System.cpp`, `Sources/src/Platform/Windows/System.cpp`, `Sources/src/Platform/Posix/System.cpp`, `Sources/src/Platform/Linux/System.cpp`, `Sources/src/Platform/MacOS/System.cpp`, `Sources/src/Platform/System.h`, `Sources/src/Platform/System.cpp`, `tools/zig/platform_system_test.cpp`, `build.zig`.

- [x] Test environment values, executable path, noninteractive dialog/URL capture, argument preservation, and child exit code on Windows; timeout/output-limit cases remain open.
- [x] Preserve the existing native Windows `CreateProcess` argument-vector path without shell command construction.
- [x] Use injectable UI callbacks so CI never opens a real browser or modal dialog.
- [ ] Convert the existing `System` facade fully behind the shared ABI while preserving UTF-8 diagnostics.
- [x] Run `zig build test-platform-system -Dtarget=x86_64-windows-msvc -Dtest-mode=run` natively on Windows; Linux/macOS execution remains in CI/native acceptance.
- [x] Commit checkpoint: `platform: complete core system services`.

**Evidence:** Windows system facade test passes injected dialog/URL capture, environment set/get, executable path, child argument `--child`, and exit code `17`.
