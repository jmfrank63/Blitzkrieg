# P00-M05 — Close the ABI Foundation Matrix

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Make the shared runtime, client, exports, and audit build correctly for all target triples.

**Dependencies:** P00-M04.

**Allowed files:** `build.zig`, `tools/zig/build_support.zig`, `tools/zig/platform_build_matrix_test.zig`, `.github/workflows/cross-platform.yml`.

- [ ] Add target policies for runtime filename, import library, ELF rpath, Mach-O install name, `.def`, subsystem, CRT, and native-run eligibility.
- [ ] Make `test-platform-foundation` depend on ABI layout, lifecycle, client, audit fixture, and target graph tests.
- [ ] Run native Windows and Linux foundation tests with `-Dtest-mode=run`.
- [ ] Compile macOS arm64 with the selected Xcode sysroot and `-Dtest-mode=compile` in CI.
- [ ] Verify Linux/macOS commands contain no Windows libraries, resources, or SDK paths.
- [ ] Commit: `build: close platform ABI foundation matrix`

**Evidence:** three target command summaries and green workflow jobs.
