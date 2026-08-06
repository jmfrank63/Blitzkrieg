# P00-M01 — Inventory the Playable Platform Surface

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Convert the current compile frontier into a deterministic audit of native headers, calls, types, libraries, and case-sensitive path failures.

**Dependencies:** Current `main`.

**Allowed files:** `tools/zig/runtime_platform_audit.zig`, `tools/zig/runtime_platform_allowlist.txt`, `tools/zig/runtime_platform_audit_test.zig`, `tools/zig/fixtures/runtime_platform/*`, `build.zig`, `docs/superpowers/evidence/platform-abstraction/inventory.md`.

- [x] Add checked-in fixtures for `windows.h`, `dinput.h`, `winsock2.h`, `HANDLE`, `SOCKET`, `GetTickCount`, `HeapAlloc`, `OutputDebugString`, and wrong-case relative includes.
- [x] Run `zig build test-runtime-platform-audit -Dtest-mode=run`; fixture output names token, file, and line.
- [x] Parse only the playable source lists and playable build functions from `build.zig`; classify every hit by service and owning future packet.
- [x] Check in the narrow current allowlist and reject unknown or newly added hits.
- [x] Run the audit on Windows and Linux and record identical sorted inventories.
- [x] Commit: `test: inventory playable platform dependencies` plus review-fix commit `7e27b4d8`.

**Evidence:** sorted inventory, fixture output, and allowlist ownership count.
