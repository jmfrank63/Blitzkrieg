# P00-M02 — Define the Versioned Platform C ABI

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Define fixed-layout results, handles, strings, callbacks, lifecycle records, and the append-only API table.

**Dependencies:** P00-M01.

**Allowed files:** `Sources/src/PlatformABI/platform_c.h`, `tools/zig/platform_abi_layout_test.cpp`, `tools/zig/platform_abi_compile_test.zig`, `build.zig`.

- [x] Write C and C++ layout tests for sizes, alignments, fixed-width fields, callbacks, and the base table.
- [x] Run `zig build test-platform-abi-layout -Dtarget=x86_64-windows-msvc -Dtest-mode=run`; Windows passes the layout executable and C import tests.
- [x] Add `BkPlatformResult`, opaque 64-bit handles, UTF-8 spans, allocator/log callbacks, create info, and the base table exactly as specified in the design.
- [x] Compile the header as C11-compatible Zig C import and C++17 on Windows. Linux/macOS validation is deferred to WSL/CI/native Mac acceptance.
- [x] Reject native headers, compiler-dependent enums, `bool`, `long`, and pointer-owned output strings by contract: the header exposes only fixed-width C types, pointers with explicit lengths, and function callbacks.
- [x] Commit: `platform: define versioned host ABI`

**Evidence:** three-target layout table and C/C++ compilation output.
