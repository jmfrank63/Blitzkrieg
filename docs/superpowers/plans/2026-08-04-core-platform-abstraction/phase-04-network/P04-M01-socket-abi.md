# P04-M01 — Define the Socket ABI

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Append opaque datagram socket, address, readiness, and error operations without exposing WinSock/POSIX types.

**Dependencies:** P01-M06.

**Allowed files:** `Sources/src/PlatformABI/platform_c.h`, `Sources/src/Platform/Core/Socket.cpp`, `Sources/src/Platform/Windows/Socket.cpp`, `Sources/src/Platform/Posix/Socket.cpp`, `tools/zig/platform_socket_types_test.cpp`, `build.zig`.

- [x] Run the existing Windows address/layout and loopback socket contracts, including open/close and conversion coverage.
- [ ] Implement process-private WinSock startup/refcount and direct POSIX socket ownership behind the new shared ABI.
- [ ] Keep socket handles generational and separate from library/thread handles.
- [x] Compile and run the existing Windows socket contracts natively.
- [x] Keep the portable socket type facade free of platform `sockaddr` and descriptor values.
- [x] Commit checkpoint: `platform: define portable socket ABI`.

**Evidence:** Windows `test-platform-socket-types` and `test-platform-network` both pass; shared runtime socket ownership remains open.
