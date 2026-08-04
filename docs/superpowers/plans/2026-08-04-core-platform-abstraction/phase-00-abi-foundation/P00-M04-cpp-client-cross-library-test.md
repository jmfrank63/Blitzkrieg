# P00-M04 — Add the C++ Client and Real Cross-Library Test

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Give legacy C++ modules checked ABI access and prove they share one runtime instance.

**Dependencies:** P00-M03.

**Allowed files:** `Sources/src/PlatformABI/PlatformClient.h`, `Sources/src/PlatformABI/PlatformClient.cpp`, `tools/zig/platform_test_consumer_a.cpp`, `tools/zig/platform_test_consumer_b.cpp`, `tools/zig/platform_client_test.cpp`, `build.zig`.

- [ ] Build two dynamic test consumers that independently call `PlatformClient` and report the observed runtime generation.
- [ ] Verify the test fails before both consumers link the shared runtime.
- [ ] Implement checked table acquisition, result helpers, bounded diagnostics, and no-throw C++ wrappers.
- [ ] Load both consumers dynamically; assert identical runtime generation and one shared lifecycle counter.
- [ ] Unload consumers before runtime destruction and assert stale client calls fail deterministically.
- [ ] Commit: `platform: add checked C++ ABI client`

**Evidence:** real DLL/SO consumer output showing one shared instance.
