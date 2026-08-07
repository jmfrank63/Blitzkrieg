# P04-M02 — Convert NetLowest to Opaque Sockets

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Remove WinSock includes and calls from `NetLowest` while preserving UDP behavior.

**Dependencies:** P04-M01.

**Allowed files:** `Sources/src/Net/StdAfx.h`, `Sources/src/Net/NetLowest.h`, `Sources/src/Net/NetLowest.cpp`, `tools/zig/netlowest_test.cpp`, `build.zig`.

- [x] Add loopback fixtures for bind, send, receive, empty nonblocking receive, oversize packet, close, and reinitialize.
- [x] Replace `SOCKET`, `WSAStartup`, `ioctlsocket`, `closesocket`, and raw `sockaddr_in` with the portable platform socket facade.
- [x] Preserve packet length, broadcast option, port byte order, and receive-address reporting.
- [ ] Build with strict Linux headers and run the fixture on Windows and Linux.
- [x] Remove `winsock2.h` from Net precompiled headers.
- [x] Commit: `net: route lowest layer through platform sockets`

**Evidence:** Windows `zig build test-netlowest -Dtarget=x86_64-windows-msvc -Dtest-mode=run` passed. The fixture verifies empty nonblocking receive, byte-identical binary payloads, deterministic 2048-byte oversize truncation, address/port reporting, close, and reinitialize. Windows `WSAEMSGSIZE` is normalized to the portable truncated byte count. Linux compilation remains open because the current configured Zig Linux C++ environment cannot locate `<cstdint>`; no Linux runtime claim is made.
