# P00-M04 — Publish Portable Compiler and Legacy Value Types

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Provide fixed compiler/export/calling-convention and non-native legacy value types without importing Windows or COM headers.

**Dependencies:** P00-M03.

**Allowed files:** `Sources/src/Platform/Compiler.h`, `Sources/src/Platform/LegacyTypes.h`, `Sources/src/Misc/Basic.h`, `Sources/src/GFX/GFXPlatform.h`, `tools/zig/platform_headers_test.cpp`, `build.zig`.

- [ ] Add a compile test asserting widths/signs for `BYTE`, `WORD`, `DWORD`, `QWORD`, `BOOL`, `RECT`, `POINT`, `GFXNativeWindow`, and `STDCALL` function pointers.
- [ ] Implement `BK_CDECL`, `BK_STDCALL`, `BK_EXPORT`, `BK_IMPORT`, `BK_NORETURN`, and `STDCALL`; retain Windows spelling on Windows and portable visibility elsewhere.
- [ ] Define legacy value types only when the platform headers do not provide them. Do not define fake `HANDLE`, `HWND`, `HMODULE`, or `SOCKET`.
- [ ] Replace `__int64` aliases and unconditional `__stdcall` in `Basic.h`; remove the historical `for` macro outside the old compiler condition.
- [ ] Keep `GFXNativeWindow` one pointer wide and document it as borrowed.
- [ ] Run `zig build test-platform-headers` for Windows and Linux; compile for macOS.
- [ ] Commit: `refactor: add portable compiler and legacy types`

**Evidence:** static-assert test output for each target.
