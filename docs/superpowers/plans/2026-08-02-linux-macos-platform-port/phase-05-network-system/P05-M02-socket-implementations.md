# P05-M02 — Implement WinSock and POSIX Socket Backends

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Provide init, UDP/TCP, nonblocking, select, error, and close behavior on all targets.

**Dependencies:** P05-M01.

**Allowed files:** `Sources/src/Platform/SocketWin32.cpp`, `Sources/src/Platform/SocketPosix.cpp`, `Sources/src/Net/NetLowest.cpp`, `tools/zig/platform_network_test.cpp`, `build.zig`.

- [ ] Test runtime init/done nesting, TCP loopback connect/send/receive, UDP loopback, nonblocking would-block, select timeout/readiness, address conversion, refused connection, and idempotent close.
- [ ] Keep `WSAStartup`/`WSACleanup`, `ioctlsocket`, and `closesocket` inside `SocketWin32.cpp`; use `fcntl`, `close`, `errno`, and POSIX select in `SocketPosix.cpp`.
- [ ] Route `NetLowest` through the facade and preserve existing zero/non-zero success conventions.
- [ ] Ignore `SIGPIPE` per socket/send operation on POSIX without changing process-global handlers where supported.
- [ ] Run native Windows/Linux loopback tests and compile macOS.
- [ ] Commit: `net: add WinSock and POSIX socket backends`

**Evidence:** TCP/UDP byte counts and normalized error table.
