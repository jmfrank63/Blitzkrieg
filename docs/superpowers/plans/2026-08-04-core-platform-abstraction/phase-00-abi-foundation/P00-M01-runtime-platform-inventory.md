# P00-M01 — Inventory the Playable Platform Surface

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Convert the current compile frontier into a deterministic audit of native headers, calls, types, libraries, and case-sensitive path failures.

**Dependencies:** Current `main`.

**Allowed files:** `tools/zig/runtime_platform_audit.zig`, `tools/zig/runtime_platform_allowlist.txt`, `tools/zig/runtime_platform_audit_test.zig`, `build.zig`, `docs/superpowers/evidence/platform-abstraction/inventory.md`.

- [ ] Add failing fixtures for `windows.h`, `dinput.h`, `winsock2.h`, `HANDLE`, `SOCKET`, `GetTickCount`, `HeapAlloc`, `OutputDebugString`, and wrong-case relative includes.
- [ ] Run `zig build test-runtime-platform-audit -Dtest-mode=run`; expect fixture failures naming token, file, and line.
- [ ] Parse only the playable source lists from `build.zig`; classify every hit by service and owning future packet.
- [ ] Check in the narrow current allowlist and reject unknown or newly added hits.
- [ ] Run the audit on Windows and Linux and record identical sorted inventories.
- [ ] Commit: `test: inventory playable platform dependencies`

**Evidence:** sorted inventory, fixture output, and allowlist ownership count.
