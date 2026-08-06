# P00-M01 runtime platform inventory

## Windows native run

- Command: `zig build test-runtime-platform-audit -Dtest-mode=run`
- Result: pass; 3 tests ran, with 2 contract tests and the inventory gate passing.
- Fixture output: all nine required fixtures reported token, file, and line.
- Inventory: 52 playable-source hits; 51 unique allowlist ownership entries.
- The allowlist rejects both unknown hits and stale entries.

Required fixture tokens:

```text
windows.h, dinput.h, winsock2.h, HANDLE, SOCKET, GetTickCount,
HeapAlloc, OutputDebugString, wrong-case relative include
```

## Linux native run

- Environment: WSL2 Ubuntu, x86_64.
- Command: `zig build --cache-dir /tmp/bk-zig-cache --global-cache-dir /tmp/bk-zig-global test-runtime-platform-audit -Dtarget=x86_64-linux-gnu -Dtest-mode=run`
- Result: pass; the same 52 hits and 51 unique allowlist ownership entries were reported.
- Note: the default target in this checkout is Windows-oriented; explicitly selecting the native Linux target is required in WSL.
