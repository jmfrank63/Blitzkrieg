# P04-M01 — Define the Socket ABI

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Append opaque datagram socket, address, readiness, and error operations without exposing WinSock/POSIX types.

**Dependencies:** P01-M06.

**Allowed files:** `Sources/src/PlatformABI/platform_c.h`, `Sources/src/Platform/Core/Socket.cpp`, `Sources/src/Platform/Windows/Socket.cpp`, `Sources/src/Platform/Posix/Socket.cpp`, `tools/zig/platform_socket_types_test.cpp`, `build.zig`.

- [ ] Test address layout, host/network conversion, invalid address, open/close, nonblocking progress, timeout, stale handle, and stable error mapping.
- [ ] Implement process-private WinSock startup/refcount and direct POSIX socket ownership.
- [ ] Keep socket handles generational and separate from library/thread handles.
- [ ] Compile C/C++ ABI tests for all triples and run native socket contracts.
- [ ] Verify no platform `sockaddr`, descriptor, or error value crosses the table.
- [ ] Commit: `platform: define portable socket ABI`

**Evidence:** result-mapping table and handle-lifecycle test.
