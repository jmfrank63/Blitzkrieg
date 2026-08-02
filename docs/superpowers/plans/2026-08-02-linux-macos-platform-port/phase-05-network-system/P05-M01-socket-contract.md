# P05-M01 — Define Portable Socket Types and Errors

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Remove WinSock types from network headers while preserving address and protocol sizes.

**Dependencies:** P04-M06.

**Allowed files:** `Sources/src/Platform/Socket.h`, `Sources/src/Net/NetLowest.h`, `Sources/src/Net/NetDriver.h`, `Sources/src/Net/NetAcks.h`, `Sources/src/Net/NetStream.h`, `tools/zig/platform_socket_types_test.cpp`, `build.zig`.

- [ ] Add static assertions for socket handle invalid value, IPv4 address/port byte order, timeout width, packet header sizes, and public network structure sizes.
- [ ] Define `SocketHandle`, `SocketError`, `SocketAddress`, and operation results without including WinSock/POSIX headers.
- [ ] Replace public `SOCKET`, `sockaddr_in`, and WinSock include usage with the portable types; keep wire structures unchanged.
- [ ] Add explicit normalized errors for would-block, interrupted, connection reset/refused, timeout, address-in-use, and unknown native error.
- [ ] Compile the types test for all targets.
- [ ] Commit: `net: define portable socket boundary`

**Evidence:** ABI size table and protocol header hash unchanged.
