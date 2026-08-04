# P04-M05 — Close the Net Module and Protocol Gate

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Load the real Net module and prove protocol compatibility across target implementations.

**Dependencies:** P04-M04.

**Allowed files:** `Sources/src/Net/GlobalsLoader.cpp`, `Sources/src/Net/NetObjectFactory.cpp`, `Sources/src/Net/net.def`, `tools/zig/net_module_test.cpp`, `tools/zig/net_protocol_fixture_test.cpp`, `tools/zig/runtime_platform_audit.zig`, `build.zig`.

- [ ] Capture representative login, acknowledgement, peer, stream, and server-info packet bytes from the Windows oracle.
- [ ] Load the real factory, exchange fixtures through loopback, and compare exact bytes and ordering.
- [ ] Target-guard `.def`, `ws2_32`, ODBC, and Windows resource/link policy.
- [ ] Run the module gate natively on Windows/Linux and compile macOS.
- [ ] Remove Net-owned WinSock/native socket tokens from the allowlist.
- [ ] Commit: `net: close portable module and protocol gate`

**Evidence:** unchanged protocol fixture hashes and three-target link audit.
