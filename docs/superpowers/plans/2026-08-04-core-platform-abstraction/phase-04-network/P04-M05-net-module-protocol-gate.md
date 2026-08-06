# P04-M05 — Close the Net Module and Protocol Gate

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Load the real Net module and prove protocol compatibility across target implementations.

**Dependencies:** P04-M04.

**Allowed files:** `Sources/src/Net/GlobalsLoader.cpp`, `Sources/src/Net/NetObjectFactory.cpp`, `Sources/src/Net/net.def`, `tools/zig/net_module_test.cpp`, `tools/zig/net_protocol_fixture_test.cpp`, `tools/zig/runtime_platform_audit.zig`, `build.zig`.

- [ ] Capture representative login, acknowledgement, peer, stream, and server-info packet bytes from the Windows oracle.
- [ ] Load the real factory, exchange fixtures through loopback, and compare exact bytes and ordering.
- [x] Target-guard `.def`, `ws2_32`, ODBC, and Windows resource/link policy.
- [x] Run the real module factory gate natively on Windows.
- [ ] Run the module gate on Linux and compile macOS.
- [x] Remove Net-owned WinSock/native socket tokens from the allowlist; platform-owned socket tokens remain explicitly assigned to P04.
- [x] Commit: `net: close portable module and protocol gate`

**Evidence:** Windows `zig build test-net-module -Dtarget=x86_64-windows-msvc -Dtest-mode=compile` built and ran the installed real `Net.dll`, validated the `Network`/`NET_NET`/`0x0100` descriptor, exact two-type factory registration, and `NET_NODE_ADDRESS` creation/release. The existing Windows NetLowest and two-peer worker fixtures provide exact transport byte checks. Oracle login/ack/peer/stream/server-info packet capture and non-Windows module execution remain open.
