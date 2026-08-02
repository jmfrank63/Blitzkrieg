# P02-M02 — Port Zig StreamIO Host File Operations

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Remove Win32 file metadata and enumeration imports from `streamio.zig`.

**Dependencies:** P02-M01.

**Allowed files:** `Sources/src/StreamIOZig/streamio.zig`, `tools/zig/streamio_platform_test.zig`, `build.zig`.

- [ ] Add temporary-directory tests for wildcard enumeration, deterministic sort, UTF-8 names, hidden/directory flags, timestamps, empty matches, separator normalization, and case-sensitive host behavior.
- [ ] Replace `GetFileAttributesExA`, `FindFirstFileA`, `FindNextFileA`, `FindClose`, and FILETIME conversion with Zig `std.Io.Dir`/file-stat operations.
- [ ] Preserve DOS date/time fields at the C ABI by converting host timestamps in Zig; clamp out-of-range dates explicitly.
- [ ] Implement wildcard matching in Zig with the existing `*`/`?` behavior and case policy defined by the storage overlay, not the host filesystem.
- [ ] Run `zig build test-streamio` and `zig build test-platform-files`.
- [ ] Commit: `streamio: use portable Zig filesystem APIs`

**Evidence:** enumeration fixture table and no Win32 extern search results.
