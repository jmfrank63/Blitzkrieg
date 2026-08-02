# P01-M03 — Centralize Diagnostics and Debugger Detection

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Remove direct `OutputDebugString` and debugger imports from shared runtime code.

**Dependencies:** P01-M01.

**Allowed files:** `Sources/src/Platform/Debug.h`, `Sources/src/Platform/Debug.cpp`, `Sources/src/Misc/ModernAssert.h`, `Sources/src/Misc/StrProc.cpp`, `tools/zig/platform_debug_test.cpp`, `build.zig`.

- [ ] Test null input, bounded formatting, newline preservation, stderr capture, and debug-mode assertion text containing expression/file/line.
- [ ] Implement `NPlatform::DebugWrite`, `DebugWriteFormat`, and `IsDebuggerAttached`; Windows may additionally call `OutputDebugStringA` privately, while other targets write stderr.
- [ ] Route `NStr::DebugTrace` and modern assertions through the facade; keep fail-fast behavior target-specific and outside public headers.
- [ ] Search allowed files for `OutputDebugString` and `IsDebuggerPresent`; expect matches only in `Debug.cpp` Windows guards.
- [ ] Commit: `platform: centralize runtime diagnostics`

**Evidence:** captured diagnostic test and search output.
