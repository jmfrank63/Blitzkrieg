# P08-M02 — PlatformRuntime Link Names and Loader Paths

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Make every module resolve the one shared PlatformRuntime in build and staged layouts.

**Dependencies:** P08-M01.

**Allowed files:** `build.zig`, `tools/zig/build_support.zig`, `tools/zig/stage.zig`, `tools/zig/verify_runtime.zig`, `tools/zig/platform_linkage_test.zig`.

- [ ] Test Windows import library/DLL, Linux SONAME and `$ORIGIN`, macOS install name and `@loader_path`, and rejection of absolute cache paths.
- [ ] Link each gameplay module to PlatformRuntime as a dynamic library, never archive its binary into static libraries.
- [ ] Stage one runtime copy with platform-correct aliases/symlinks created by Zig APIs.
- [ ] Verify Game and every module resolve that staged copy without environment variables.
- [ ] Run linkage inspection on all produced binaries.
- [ ] Commit: `build: link one shared platform runtime`

**Evidence:** dependency tables and staged resolution report.
