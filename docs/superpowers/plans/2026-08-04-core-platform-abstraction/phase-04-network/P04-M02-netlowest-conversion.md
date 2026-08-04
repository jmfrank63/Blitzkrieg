# P04-M02 — Convert NetLowest to Opaque Sockets

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Remove WinSock includes and calls from `NetLowest` while preserving UDP behavior.

**Dependencies:** P04-M01.

**Allowed files:** `Sources/src/Net/StdAfx.h`, `Sources/src/Net/NetLowest.h`, `Sources/src/Net/NetLowest.cpp`, `tools/zig/netlowest_test.cpp`, `build.zig`.

- [ ] Add loopback fixtures for bind, send, receive, empty nonblocking receive, oversize packet, close, and reinitialize.
- [ ] Replace `SOCKET`, `WSAStartup`, `ioctlsocket`, `closesocket`, and raw `sockaddr_in` with PlatformClient calls.
- [ ] Preserve packet length, broadcast option, port byte order, and receive-address reporting.
- [ ] Build with strict Linux headers and run the fixture on Windows and Linux.
- [ ] Remove `winsock2.h` from Net precompiled headers.
- [ ] Commit: `net: route lowest layer through platform sockets`

**Evidence:** byte-identical loopback packet fixture.
