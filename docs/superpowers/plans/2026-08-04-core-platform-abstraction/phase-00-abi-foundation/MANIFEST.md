# Phase 00 — ABI Foundation

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Establish the checked platform boundary, versioned ABI, single shared runtime, client wrapper, and three-target foundation gate.

| Packet | Depends on | Owns |
|---|---|---|
| P00-M01 | current `main` | playable-source inventory and audit baseline |
| P00-M02 | M01 | public C ABI types and layout tests |
| P00-M03 | M02 | shared runtime lifecycle and state |
| P00-M04 | M03 | C++ client and real shared-library consumer |
| P00-M05 | M04 | target graph and foundation matrix |

P00-M01 evidence: commits `f27534cab` and `7e27b4d8`; Windows native audit passed with 9 tests and 68 inventory hits / 67 unique allowlist entries. Linux/WSL2 verification and byte-identical sorted inventory evidence are recorded in `docs/superpowers/evidence/platform-abstraction/inventory.md`.

P00-M02 Windows evidence: `test-platform-abi-layout -Dtarget=x86_64-windows-msvc -Dtest-mode=run` passed the C++17 layout executable and Zig C-import test. Linux/macOS target validation remains deferred to WSL/CI/native Mac acceptance.

P00-M03 Windows evidence: `test-platform-runtime -Dtarget=x86_64-windows-msvc -Dtest-mode=run` passed the shared DLL lifecycle test. The export definition contains only `bk_platform_get_api`; two create/destroy cycles complete without SDL, socket, thread, or window state.

P00-M04 Windows evidence: `test-platform-client -Dtarget=x86_64-windows-msvc -Dtest-mode=run` dynamically loaded consumer A and B, both observed shared runtime generation `1`, then unloaded consumers after runtime destruction.

Exit: `test-platform-abi` and `test-platform-foundation` pass for native Windows/Linux and compile for macOS arm64, with a versioned shared runtime used by a real C++ consumer.
