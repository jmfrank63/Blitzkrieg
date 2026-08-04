# P00-M02 — Define the Versioned Platform C ABI

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Define fixed-layout results, handles, strings, callbacks, lifecycle records, and the append-only API table.

**Dependencies:** P00-M01.

**Allowed files:** `Sources/src/PlatformABI/platform_c.h`, `tools/zig/platform_abi_layout_test.cpp`, `tools/zig/platform_abi_compile_test.zig`, `build.zig`.

- [ ] Write failing C and C++ layout tests for sizes, alignments, nullability, calling convention, and version negotiation.
- [ ] Run `zig build test-platform-abi-layout -Dtarget=x86_64-windows-msvc -Dtest-mode=run`; expect missing declarations.
- [ ] Add `BkPlatformResult`, opaque 64-bit handles, UTF-8 spans, allocator/log callbacks, create info, and the base table exactly as specified in the design.
- [ ] Compile the header as C11 and C++17 for Windows, Linux, and macOS arm64.
- [ ] Reject native headers, enums with compiler-dependent size, `bool`, `long`, and pointer-owned output strings in the ABI test.
- [ ] Commit: `platform: define versioned host ABI`

**Evidence:** three-target layout table and C/C++ compilation output.
