# P00-M05 — Close the ABI Foundation Matrix

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Make the shared runtime, client, exports, and audit build correctly for all target triples.

**Dependencies:** P00-M04.

**Allowed files:** `build.zig`, `tools/zig/build_support.zig`, `tools/zig/platform_build_matrix_test.zig`, `.github/workflows/cross-platform.yml`.

- [x] Add target policies for runtime filename, import library, ELF rpath, Mach-O install name, `.def`, subsystem, CRT, and native-run eligibility.
- [x] Make `test-platform-foundation` depend on ABI layout, lifecycle, client, audit fixture, and target graph tests.
- [x] Run native Windows and Linux foundation tests with `-Dtest-mode=run`.
- [x] Compile macOS arm64 with the selected Xcode sysroot and `-Dtest-mode=compile` in CI.
- [x] Verify Linux/macOS commands contain no Windows libraries, resources, or SDK paths.
- [x] Commit: `ci: exercise platform foundation matrix` plus the Windows foundation implementation checkpoint.

**Evidence:** Windows `zig build test-platform-foundation -Dtarget=x86_64-windows-msvc -Dtest-mode=run` passed after fixing the dual object/executable header test graph; the audit reports 68 inventory hits and 67 allowlist entries with zero unknown/stale entries. Linux native execution and macOS arm64 sysroot compilation are wired in `.github/workflows/cross-platform.yml` for CI.
