# P03-M06 — Close the Input Shared-Module Gate

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Build and load the real Input module on all targets with no DirectInput dependency in the portable graph.

**Dependencies:** P03-M05.

**Allowed files:** `Sources/src/Input/GlobalsLoader.cpp`, `Sources/src/Input/StdAfx.h`, `Sources/src/Input/InputObjectFactory.cpp`, `Sources/src/Input/Input.def`, `tools/zig/input_module_test.cpp`, `tools/zig/runtime_platform_audit.zig`, `build.zig`.

- [ ] Load the real Input factory, initialize it against PlatformRuntime, inject the full fixture corpus, and shut it down twice; the compile gate is in place, while native execution is blocked by the host's MSVC debug CRT loader (`0xc0000139`).
- [ ] Target-guard `.def`, import libraries, COM support, DirectInput libraries, and Windows-only oracle source files.
- [ ] Run `zig build test-input-module -Dtest-mode=run` natively on Windows and Linux; compile for macOS.
- [x] Verify the non-Windows Input build graph does not add `dinput8`, `dxguid`, `winmm`, or `user32`; Windows still retains those temporary oracle links.
- [ ] Remove all Input-owned native tokens from the audit allowlist.
- [ ] Commit: `input: close portable module gate`

**Evidence:** `test-input-module` compiles for `x86_64-windows-msvc` and wires the real `GetModuleDescriptor`/factory lifecycle test. A Windows run reaches the loader but exits `0xc0000139` because the installed host debug CRT does not expose the MSVC symbols used by the generated StreamIO dependency; this is an environment/runtime packaging blocker, not a compile failure. The Linux/macOS link policy remains guarded in `build.zig`; the Windows DirectInput/oracle link cleanup remains open.
